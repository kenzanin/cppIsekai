#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>
using namespace sqlite_orm;

Store::Store(const std::string& path) : impl_(std::make_unique<StoreImpl>(path)) {
    impl_->storage.pragma.journal_mode(journal_mode::WAL);
    // wait on writers instead of failing instantly: raw api_cache opens and this
    // ORM connection are concurrent writers (db-lock regression).
    impl_->storage.pragma.busy_timeout(3000);
    // FK: pragma_t has no foreign_keys setter in v1.9.1; make_storage enables it
    // automatically because the schema declares a foreign_key.
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
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table; add the saved-manga stat columns additively here.
    {
        const auto cols = impl_->storage.pragma.table_info("saved_manga");
        std::unordered_set<std::string> have;
        for (const auto& c : cols) {
            have.insert(c.name);
        }
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            constexpr std::string_view stmts[] = {
                "ALTER TABLE saved_manga ADD COLUMN status TEXT NOT NULL DEFAULT 'ongoing'",
                "ALTER TABLE saved_manga ADD COLUMN total_ch INTEGER NOT NULL DEFAULT 0",
                "ALTER TABLE saved_manga ADD COLUMN read_ch INTEGER NOT NULL DEFAULT 0",
                "ALTER TABLE saved_manga ADD COLUMN has_update INTEGER NOT NULL DEFAULT 0",
                "ALTER TABLE saved_manga ADD COLUMN last_read_at INTEGER NOT NULL DEFAULT 0",
                "ALTER TABLE saved_manga ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0",
                "ALTER TABLE saved_manga ADD COLUMN synopsis TEXT NOT NULL DEFAULT ''",
                "ALTER TABLE saved_manga ADD COLUMN alt_titles TEXT NOT NULL DEFAULT ''",
                "ALTER TABLE saved_manga ADD COLUMN dup_group INTEGER NOT NULL DEFAULT 0",
            };
            constexpr std::string_view names[] = {
                "status",     "total_ch", "read_ch",    "has_update", "last_read_at",
                "updated_at", "synopsis", "alt_titles", "dup_group",
            };
            for (std::size_t i = 0; i < std::size(stmts); ++i) {
                if (!have.contains(std::string(names[i]))) {
                    sqlite3_exec(db, stmts[i].data(), nullptr, nullptr, nullptr);
                }
            }
            sqlite3_close(db); // one-shot migration alongside the ORM connection
        }
    }
    // library-dedup 1.3: standalone FTS5 index (content not needed; small N).
    // Amalgamation without FTS5 -> warn once, dedup stays disabled (dup_group=0).
    {
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            sqlite3_busy_timeout(db, 3000);
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
