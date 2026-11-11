#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <stdexcept>
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
        for (const auto& c : cols) has_payload = has_payload || c.name == "payload";
        sqlite3* db = nullptr;
        if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
            if (!has_payload)
                sqlite3_exec(db, "ALTER TABLE api_cache ADD COLUMN payload BLOB", nullptr, nullptr, nullptr);
            // jsonb_valid CHECK can't be added post-hoc to an existing column
            // cheaply; enforce at write time (jsonb() rejects malformed input).
            sqlite3_close(db);
        }
    }
    // ponytail: sqlite_orm sync_schema won't ALTER an existing table; add zoom additively here.
    {
        const auto cols = impl_->storage.pragma.table_info("reader_positions");
        bool has_zoom = false;
        for (const auto& c : cols) has_zoom = has_zoom || c.name == "zoom";
        if (!has_zoom) {
            sqlite3* db = nullptr;
            sqlite3_open(path.c_str(), &db);
            sqlite3_exec(db, "ALTER TABLE reader_positions ADD COLUMN zoom TEXT DEFAULT ''", nullptr, nullptr, nullptr);
            sqlite3_close(db); // one-shot migration alongside the ORM connection
        }
    }


}

Store::~Store() = default;

