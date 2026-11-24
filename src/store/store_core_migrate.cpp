#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>
using namespace sqlite_orm;

// The saved_manga column migration (design D5), ordered and idempotent:
//   (a) add the stat columns that are missing (status_flags among them),
//   (b) backfill status_flags from the columns that still exist -- has_update
//       (the New state), total_ch/read_ch (Done) and status (category) --
//       while preserving bit6 (is_dim),
//   (c) drop has_update: bit0 is the single New source from here on.
// The live library has no status_flags column while 18/31 rows carry
// has_update=1, so (c) must never run before (b).
//
// ponytail: (c) needs SQLite >= 3.35 (repo builds 3.54); on an older engine
// the drop fails and the column stays (harmless: bit0 already mirrors it).
void migrate_saved_manga(const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return;
    }
    std::unordered_set<std::string> have;
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "PRAGMA table_info(saved_manga)", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
                if (name != nullptr) {
                    have.emplace(name);
                }
            }
            sqlite3_finalize(st);
        }
    }
    constexpr std::string_view stmts[] = {
        "ALTER TABLE saved_manga ADD COLUMN status TEXT NOT NULL DEFAULT 'ongoing'",
        "ALTER TABLE saved_manga ADD COLUMN total_ch INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN read_ch INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN last_read_at INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN synopsis TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN alt_titles TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN dup_group INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN status_flags INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN authors TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN artists TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN tags TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN synopsis_display TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN chapters_fp TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE saved_manga ADD COLUMN novel INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE saved_manga ADD COLUMN detail_url TEXT NOT NULL DEFAULT ''",
    };
    constexpr std::string_view names[] = {
        "status",     "total_ch", "read_ch",    "last_read_at", "updated_at",
        "synopsis",   "alt_titles", "dup_group", "status_flags",
        "authors", "artists", "tags", "synopsis_display", "pinned",
        "chapters_fp", "novel", "detail_url",
    };
    for (std::size_t i = 0; i < std::size(stmts); ++i) {
        if (!have.contains(std::string(names[i]))) {
            sqlite3_exec(db, stmts[i].data(), nullptr, nullptr, nullptr);
        }
    }
    if (have.contains("has_update")) {
        // (b) backfill every row, preserving bit6 (is_dim)
        sqlite3_exec(db, "UPDATE saved_manga SET status_flags = "
                          "(CASE WHEN has_update <> 0 AND NOT (total_ch > 0 AND read_ch >= total_ch) THEN 1 ELSE 0 END) "
                          "| (CASE WHEN total_ch > 0 AND read_ch >= total_ch THEN 2 ELSE 0 END) "
                          "| (CASE lower(status) WHEN 'ongoing' THEN 4 WHEN 'hiatus' THEN 8 "
                          "WHEN 'canceled' THEN 16 WHEN 'cancelled' THEN 16 "
                          "WHEN 'completed' THEN 32 ELSE 0 END) "
                          "| (status_flags & 64)",
                      nullptr, nullptr, nullptr);
        // (c) the column is dead now; irreversible on purpose
        sqlite3_exec(db, "ALTER TABLE saved_manga DROP COLUMN has_update", nullptr, nullptr, nullptr);
    }
    sqlite3_close(db); // one-shot migration alongside the ORM connection
}
