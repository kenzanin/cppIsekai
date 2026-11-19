#pragma once
/** \file store_ops.hpp
 *  \brief Serializes every Store operation on the one shared connection and
 *  names the op that holds it.
 *
 * The store owns a single sqlite handle (store.hpp) and sqlite_orm runs each
 * operation as several statements, so two threads interleaving statements on
 * that handle produce "database is locked" from SQLITE_BUSY_SNAPSHOT -- a code
 * the busy handler never sees, which is why store_busy's retry log stayed
 * silent while browse requests failed instantly. OpTrace therefore takes a
 * process-wide recursive mutex for the duration of the call, and logs the op
 * with its wall time.
 *
 * Levels: per-op lines are \c debug (silent by default, nothing to delete --
 * set `[log] level = debug` / CPPISEKAI_LOG_LEVEL=debug to read them). Warns:
 * an op slower than kSlowOpMs, or two threads inside the ORM connection at once
 * (only possible if a method bypasses this guard).
 */
#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace store_detail {

/// Wall time above which a finished op warns.
inline constexpr double kSlowOpMs = 50.0;

/// RAII over one explicit sqlite transaction: rolls back when the scope is
/// left without commit(). Without it a throw between BEGIN and COMMIT leaves
/// the transaction open on the one shared connection, and every later BEGIN
/// fails with "cannot start a transaction within a transaction" -- a wedged
/// daemon that only a restart cures.
template <typename Storage>
class TxGuard {
public:
    explicit TxGuard(Storage& st) : st_(st) {}
    TxGuard(const TxGuard&) = delete;
    TxGuard& operator=(const TxGuard&) = delete;
    ~TxGuard() {
        if (done_) {
            return;
        }
        try {
            st_.rollback();
        } catch (const std::exception& e) {
            spdlog::error("store: rollback failed: {}", e.what());
        }
    }
    void commit() {
        st_.commit();
        done_ = true;
    }

private:
    Storage& st_;
    bool done_ = false;
};

inline std::string tid_str() {
    return std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
}

/// RAII scope over one Store operation: serializes it, names it, times it.
class OpTrace {
public:
    explicit OpTrace(const char* op) : op_(op), t0_(std::chrono::steady_clock::now()), lock_() {
        // api_cache_*/maintenance open their own sqlite handle (raw): they never
        // touch the shared ORM connection, so serializing them behind db_mutex
        // only added queue wait (store-contention C2). One mutex per handle
        // family; the overlap ledger below still tracks both.
        const bool raw = std::strncmp(op_, "api_cache_", 10) == 0 ||
                         std::strcmp(op_, "maintenance_vacuum_backup") == 0 ||
                         std::strcmp(op_, "cache_prune_expired") == 0;
        lock_ = std::unique_lock<std::recursive_mutex>(raw ? raw_db_mutex() : db_mutex());
        std::lock_guard<std::mutex> lock(mutex());
        for (const auto& [tid, name, other_raw] : active()) {
            if (tid != std::this_thread::get_id()) {
                const std::string msg = "store: overlapping access -- " + std::string(op_) + " (t" + tid_str() +
                                        ") while " + name + " (t" +
                                        std::to_string(std::hash<std::thread::id>{}(tid) % 10000) + ") is in flight";
                if (!raw && !other_raw) {
                    spdlog::warn("{} [same connection, two threads]", msg);
                } else {
                    spdlog::debug("{}", msg);
                }
                break;
            }
        }
        active().emplace_back(std::this_thread::get_id(), op_, raw);
    }

    ~OpTrace() {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_).count();
        if (ms >= kSlowOpMs) {
            spdlog::warn("store: {} took {:.0f} ms (t{})", op_, ms, tid_str());
        } else {
            spdlog::debug("store: {} {:.1f} ms (t{})", op_, ms, tid_str());
        }
        std::lock_guard<std::mutex> lock(mutex());
        auto& a = active();
        for (std::size_t i = a.size(); i > 0; --i) {
            if (std::get<0>(a[i - 1]) == std::this_thread::get_id() && std::get<1>(a[i - 1]) == op_) {
                a.erase(a.begin() + static_cast<std::ptrdiff_t>(i - 1));
                break;
            }
        }
    }

    OpTrace(const OpTrace&) = delete;
    OpTrace& operator=(const OpTrace&) = delete;

private:
    /// One recursive mutex for the whole process: the connection is not
    /// shareable across threads, and Store methods may nest.
    static std::recursive_mutex& db_mutex() {
        static std::recursive_mutex m;
        return m;
    }
    /// Mutex for the raw-handle ops (own sqlite3_open connection, C2).
    static std::recursive_mutex& raw_db_mutex() {
        static std::recursive_mutex m;
        return m;
    }
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }
    static std::vector<std::tuple<std::thread::id, const char*, bool>>& active() {
        static std::vector<std::tuple<std::thread::id, const char*, bool>> a;
        return a;
    }

    const char* op_;
    std::chrono::steady_clock::time_point t0_;
    std::unique_lock<std::recursive_mutex> lock_;
};

} // namespace store_detail
