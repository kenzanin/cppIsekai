#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"
#include "store_ops.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <vector>

// [backup] maintenance: the raw-connection pattern from store_api.cpp — a
// second short-lived handle keeps the orm connection untouched; VACUUM INTO
// reads a consistent snapshot without blocking writers (WAL).
std::string Store::maintenance_vacuum_backup(const std::string& backup_dir, int keep) {
    const store_detail::OpTrace _op{"maintenance_vacuum_backup"};
    std::error_code ec;
    std::filesystem::create_directories(backup_dir, ec);
    if (ec) {
        spdlog::warn("[backup] mkdir {}: {}", backup_dir, ec.message());
        return {};
    }

    char ts[20];
    {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        if (localtime_r(&now, &tm) == nullptr || std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tm) == 0) {
            spdlog::warn("[backup] timestamp build failed");
            return {};
        }
    }
    const std::string target = backup_dir + "/library-" + ts + ".db";
    // same-second rerun (tests, manual + scheduler race): VACUUM INTO fails on
    // an existing target, so overwrite is deliberate.
    std::filesystem::remove(target, ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
        }
        spdlog::warn("[backup] open {} failed", impl_->path);
        return {};
    }
    store_detail::attach_busy(db);

    // 1) defrag the live db — best effort: a busy moment skips it, the
    //    snapshot below still happens this run.
    char* err = nullptr;
    if (sqlite3_exec(db, "VACUUM", nullptr, nullptr, &err) != SQLITE_OK) {
        spdlog::warn("[backup] live VACUUM deferred: {}", err ? err : "?");
        sqlite3_free(err);
    }

    // 2) consistent snapshot copy.
    sqlite3_stmt* stmt = nullptr;
    bool ok = sqlite3_prepare_v2(db, "VACUUM INTO ?", -1, &stmt, nullptr) == SQLITE_OK;
    if (ok) {
        ok = sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(stmt) == SQLITE_DONE;
    }
    const std::string msg = ok ? "" : sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (!ok) {
        spdlog::warn("[backup] snapshot {} failed: {}", target, msg);
        std::filesystem::remove(target, ec);
        return {};
    }

    // 3) prune: name embeds the timestamp, lexical sort == age order.
    if (keep < 1) {
        keep = 1;
    }
    std::vector<std::filesystem::path> snaps;
    for (const auto& it : std::filesystem::directory_iterator(backup_dir, ec)) {
        if (!ec && it.is_regular_file() && it.path().filename().string().rfind("library-", 0) == 0 &&
            it.path().extension() == ".db") {
            snaps.push_back(it.path());
        }
    }
    std::sort(snaps.begin(), snaps.end());
    const auto keep_n = static_cast<std::size_t>(keep);
    if (snaps.size() > keep_n) {
        // newest are the lexicographically largest: drop from the front
        const auto drop = snaps.size() - keep_n;
        for (std::size_t i = 0; i < drop; ++i) {
            std::filesystem::remove(snaps[i], ec);
        }
    }
    return target;
}

// hot-path-perf P3/D3: api_cache had no age-based reaper (rows left only on
// unlink/purge), so expired rows plus dead-plugin rows accumulated forever.
// Horizon = expiry + 7d keeps the stale-serve grace and per-plugin manifest
// cache_ttl intact; a busy/failing sweep just retries next tick.
std::size_t Store::cache_prune_expired() {
    const store_detail::OpTrace _op{"cache_prune_expired"};
    const std::int64_t horizon = static_cast<std::int64_t>(std::time(nullptr)) - 7 * 86400;
    sqlite3* db = impl_->raw_db();
    if (db == nullptr) {
        return 0;
    }
    sqlite3_stmt* stmt = nullptr;
    bool ok = sqlite3_prepare_v2(db, "DELETE FROM api_cache WHERE fetched_at + ttl_s < ?", -1, &stmt, nullptr) == SQLITE_OK;
    std::size_t n = 0;
    if (ok) {
        ok = sqlite3_bind_int64(stmt, 1, horizon) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_DONE;
        n = ok ? static_cast<std::size_t>(sqlite3_changes(db)) : 0;
    }
    const std::string msg = ok ? "" : sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (!ok) {
        spdlog::warn("[backup] cache prune failed: {}", msg);
    }
    return n;
}
