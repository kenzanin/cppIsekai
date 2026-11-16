#pragma once
/** \file store_busy.hpp
 *  \brief Busy-wait policy shared by every sqlite connection in the process.
 *
 * The library DB is one file written by several connections (the sqlite_orm
 * storage connection plus the raw api_cache/FTS connections). WAL allows a
 * single writer at a time, so a slow writer hands SQLITE_BUSY to the others.
 * A bare `sqlite3_busy_timeout` gives up silently and the caller surfaces it as
 * a 500 mid-page; this handler keeps the same retry shape but logs the wait so
 * a real lock-holder is visible in the log instead of only in the symptom.
 */
#include <spdlog/spdlog.h>
#include <sqlite3.h>

namespace store_detail {

inline constexpr int kBusyStepMs = 50; ///< sleep between retries
inline constexpr int kBusyMs = 15000;  ///< give up after this much wall time

/// Retry callback in the sqlite3_busy_handler shape: `1` retry, `0` give up.
inline int busy_wait(int count) {
    const int waited = count * kBusyStepMs;
    if (waited == 1000 || waited == 3000) {
        spdlog::warn("store: db busy, waited {} ms", waited);
    }
    if (waited >= kBusyMs) {
        spdlog::error("store: db busy gave up after {} ms", waited);
        return 0;
    }
    sqlite3_sleep(kBusyStepMs);
    return 1;
}

/// Attach the policy to a raw handle (`sqlite3_busy_timeout` cannot log).
inline void attach_busy(sqlite3* db) {
    sqlite3_busy_handler(
        db, [](void*, int count) { return busy_wait(count); }, nullptr);
}

} // namespace store_detail
