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

namespace {
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
} // namespace

Store::Store(const std::string& path) : impl_(std::make_unique<StoreImpl>(path)) {
    impl_->storage.pragma.journal_mode(journal_mode::WAL);
    // store-contention C4 (user-approved): WAL's standard pairing. App-crash safe;
    // power-loss may drop the last txn(s) -- bounded by the hourly [backup] snapshot.
    impl_->storage.pragma.synchronous(1); // 1 = NORMAL
    // wait on writers instead of failing instantly: raw api_cache opens and this
    // ORM connection are concurrent writers (db-lock regression).
    impl_->storage.busy_handler([](int count) { return store_detail::busy_wait(count); });
    // Hold the ORM connection open for the storage lifetime. sqlite_orm's lazy
    // open/close per use is a documented data race with concurrent raw-handle
    // writers: the reopen's on_open pragmas run before the busy handler is
    // attached and surface SQLITE_BUSY as a thrown std::system_error
    // (test_store 2.2 abort, state_set i0). Also skips per-op open/close churn.
    impl_->storage.open_forever();
    // FK: pragma_t has no foreign_keys setter in v1.9.1; make_storage enables it
    // automatically because the schema declares a foreign_key.
    //
    // The saved_manga migration runs BEFORE sync_schema: sqlite_orm drops db
    // columns the storage schema does not map, so an unmapped has_update would
    // be dropped before the backfill could read it (design D5).
    migrate_saved_manga(path);
    impl_->storage.sync_schema();
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table or add a
    // JSONB blob column; payload is created/dropped alongside the ORM schema
    // here (raw connection, same one-shot pattern as reader_positions.zoom).
    {
        const auto cols = impl_->storage.pragma.table_info("api_cache");
        bool has_payload = false;
        for (const auto& c : cols) {
            has_payload = has_payload || c.name == "payload";
        }
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            if (!has_payload) {
                sqlite3_exec(db, "ALTER TABLE api_cache ADD COLUMN payload BLOB", nullptr, nullptr, nullptr);
            }
            // jsonb_valid CHECK can't be added post-hoc to an existing column
            // cheaply; enforce at write time (jsonb() rejects malformed input).
            sqlite3_close(db);
        }
    }
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table; the
    // saved-manga stat columns are added additively in migrate_saved_manga().
    // library-promote-on-save: chapters.release_at (verbatim epoch/ISO text)
    // added additively here for the same reason.
    {
        const auto cols = impl_->storage.pragma.table_info("chapters");
        bool has_release = false;
        for (const auto& c : cols) {
            has_release = has_release || c.name == "release_at";
        }
        if (!has_release) {
            sqlite3* db = nullptr;
            sqlite3_open(path.c_str(), &db);
            sqlite3_exec(db, "ALTER TABLE chapters ADD COLUMN release_at TEXT NOT NULL DEFAULT ''", nullptr, nullptr,
                         nullptr);
            sqlite3_close(db); // one-shot migration alongside the ORM connection
        }
        bool has_read_max = false;
        for (const auto& c : cols) {
            has_read_max = has_read_max || c.name == "read_max";
        }
        if (!has_read_max) {
            sqlite3* db = nullptr;
            sqlite3_open(path.c_str(), &db);
            sqlite3_exec(db, "ALTER TABLE chapters ADD COLUMN read_max INTEGER NOT NULL DEFAULT 0", nullptr, nullptr,
                         nullptr);
            sqlite3_close(db);
        }
        // library-promote-on-save 4.4: the host/skip|read|total chapter shadows
        // are retired -- flags/read_max on the master row are the single truth.
        // Backfill read progress once from the shadows, then drop the keys;
        // both statements no-op once the shadows are gone.
        {
            sqlite3* db = nullptr;
            if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
                store_detail::attach_busy(db);
                sqlite3_exec(db,
                             "UPDATE chapters SET read_max = (SELECT CAST(s.value AS INTEGER) FROM plugin_state s"
                             " WHERE s.plugin_id = 'host' AND s.key = 'host/read/' || chapters.chapter_id)"
                             " WHERE read_max = 0 AND EXISTS(SELECT 1 FROM plugin_state s"
                             " WHERE s.plugin_id = 'host' AND s.key = 'host/read/' || chapters.chapter_id)",
                             nullptr, nullptr, nullptr);
                sqlite3_exec(db,
                             "DELETE FROM plugin_state WHERE plugin_id = 'host' AND (key LIKE 'host/skip/%' OR"
                             " key LIKE 'host/read/%' OR key LIKE 'host/total/%')",
                             nullptr, nullptr, nullptr);
                sqlite3_close(db);
            }
        }
    }
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table; add zoom additively here.
    {
        const auto cols = impl_->storage.pragma.table_info("reader_positions");
        bool has_zoom = false;
        for (const auto& c : cols) {
            has_zoom = has_zoom || c.name == "zoom";
        }
        if (!has_zoom) {
            sqlite3* db = nullptr;
            sqlite3_open(path.c_str(), &db);
            sqlite3_exec(db, "ALTER TABLE reader_positions ADD COLUMN zoom TEXT DEFAULT ''", nullptr, nullptr, nullptr);
            sqlite3_close(db); // one-shot migration alongside the ORM connection
        }
    }
    // library-ownership 4.2: the legacy title cache table is retired.
    {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            store_detail::attach_busy(db);
            sqlite3_exec(db, "DROP TABLE IF EXISTS title", nullptr, nullptr, nullptr);
            sqlite3_close(db);
        }
    }
    // library-ownership 4.1: per-pair plugin_state families move into columns,
    // and the retired shadow key families go with them. Idempotent: the UPDATE
    // joins on the keys' existence and the DELETEs no-op once empty.
    {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            store_detail::attach_busy(db);
            sqlite3_exec(db,
                         "UPDATE saved_manga SET chapters_fp = (SELECT s.value FROM plugin_state s"
                         " WHERE s.plugin_id = 'host' AND s.key = 'host/chapters_fp/' || saved_manga.plugin || '/' || saved_manga.slug)"
                         " WHERE EXISTS(SELECT 1 FROM plugin_state s WHERE s.plugin_id = 'host'"
                         " AND s.key = 'host/chapters_fp/' || saved_manga.plugin || '/' || saved_manga.slug)",
                         nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                         "UPDATE saved_manga SET novel = 1 WHERE EXISTS(SELECT 1 FROM plugin_state s"
                         " WHERE s.plugin_id = 'host' AND s.key = 'host/novel/' || saved_manga.plugin || '/' || saved_manga.slug"
                         " AND CAST(s.value AS INTEGER) = 1)",
                         nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                         "UPDATE saved_manga SET detail_url = (SELECT s.value FROM plugin_state s"
                         " WHERE s.plugin_id = 'host' AND s.key = 'host/detail_url/' || saved_manga.plugin || '/' || saved_manga.slug)"
                         " WHERE EXISTS(SELECT 1 FROM plugin_state s WHERE s.plugin_id = 'host'"
                         " AND s.key = 'host/detail_url/' || saved_manga.plugin || '/' || saved_manga.slug)",
                         nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                         "DELETE FROM plugin_state WHERE plugin_id = 'host' AND (key LIKE 'host/chapters_fp/%' OR"
                         " key LIKE 'host/novel/%' OR key LIKE 'host/detail_url/%' OR key LIKE 'host/skip/%' OR"
                         " key LIKE 'host/read/%' OR key LIKE 'host/total/%')",
                         nullptr, nullptr, nullptr);
            sqlite3_close(db);
        }
    }
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table; the
    // saved-manga stat columns are added additively in migrate_saved_manga().
    // library-dedup 1.3: standalone FTS5 index (content not needed; small N).
    // Amalgamation without FTS5 -> warn once, dedup stays disabled (dup_group=0).
    {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            store_detail::attach_busy(db);
            const char* ddl = "CREATE VIRTUAL TABLE IF NOT EXISTS library_fts"
                              " USING fts5(plugin UNINDEXED, slug UNINDEXED, name, alt_titles, synopsis)";
            char* err = nullptr;
            if (sqlite3_exec(db, ddl, nullptr, nullptr, &err) == SQLITE_OK) {
                impl_->fts5_ok = true;
            } else {
                spdlog::warn("[store] FTS5 unavailable ({}); library dedup disabled, dup_group stays 0",
                             (err != nullptr) ? err : "unknown error");
                sqlite3_free(err);
            }
            sqlite3_close(db);
        } else {
            spdlog::warn("[store] cannot open {} for FTS5 probe; library dedup disabled", path);
        }
    }
}

Store::~Store() = default;

void Store::mark_dup_dirty() {
    impl_->dup_dirty.store(true, std::memory_order_relaxed);
}

bool Store::take_dup_dirty() {
    return impl_->dup_dirty.exchange(false, std::memory_order_relaxed);
}
