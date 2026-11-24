#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <utf8proc.h>

using namespace sqlite_orm;

namespace store_detail {
// FTS maintenance, decl in store_internal.hpp: the saved writers keep
// name/alt_titles/synopsis indexed (library-dedup 1.4).
void fts_sync(StoreImpl& impl, const std::string& plugin, const std::string& slug, const std::string& name,
              const std::string& alt_titles, const std::string& synopsis) {
    if (!impl.fts5_ok) {
        return;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(impl.path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return;
    }
    store_detail::attach_busy(db);
    { // tokenized text: name + alt titles (raw JSON) + synopsis; FTS5 tokenizes free text
        sqlite3_stmt* del = nullptr;
        const char* dsql = "DELETE FROM library_fts WHERE plugin=? AND slug=?";
        if (sqlite3_prepare_v2(db, dsql, -1, &del, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(del, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        sqlite3_stmt* ins = nullptr;
        const char* isql = "INSERT INTO library_fts(plugin, slug, name, alt_titles, synopsis) VALUES(?,?,?,?,?)";
        if (sqlite3_prepare_v2(db, isql, -1, &ins, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(ins, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 3, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 4, alt_titles.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins, 5, synopsis.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(ins);
            sqlite3_finalize(ins);
        }
    }
    sqlite3_close(db);
}
void fts_delete(StoreImpl& impl, const std::string& plugin, const std::string& slug) {
    if (!impl.fts5_ok) {
        return;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(impl.path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return;
    }
    store_detail::attach_busy(db);
    sqlite3_stmt* del = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM library_fts WHERE plugin=? AND slug=?", -1, &del, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(del, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(del, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }
    sqlite3_close(db);
}
} // namespace store_detail

