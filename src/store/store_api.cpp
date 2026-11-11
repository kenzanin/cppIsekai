#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;

std::optional<ApiCacheRow> Store::api_cache_get(const std::string& plugin_id, const std::string& query_hash) {
    // Raw connection: payload is JSONB binary — sqlite_orm has no mapping for it.
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) return std::nullopt;
    sqlite3_busy_timeout(db, 3000);
    sqlite3_stmt* stmt = nullptr;
    std::optional<ApiCacheRow> row;
    const char* sql = "SELECT params_json, json(payload), fetched_at, ttl_s FROM api_cache "
                      "WHERE plugin_id = ? AND query_hash = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, plugin_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, query_hash.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* params = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const int plen = sqlite3_column_bytes(stmt, 1);
            // Empty/NULL payload (e.g. row written before payload existed) is a
            // row-shaped miss, not a fresh hit -- otherwise the caller serves
            // a 200 with an empty body.
            if (payload && plen > 0)
                row = ApiCacheRow{params ? params : "", std::string(payload, static_cast<std::size_t>(plen)),
                                  sqlite3_column_int64(stmt, 2), sqlite3_column_int64(stmt, 3)};
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return row;
}

void Store::api_cache_put(const std::string& plugin_id, const std::string& query_hash,
                          const std::string& params_json, const std::string& payload,
                          std::int64_t fetched_at, std::int64_t ttl_s) {
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) return;
    sqlite3_busy_timeout(db, 3000);
    sqlite3_stmt* stmt = nullptr;
    // jsonb(?) rejects malformed JSON -> the write fails loudly, never stores garbage.
    const char* sql = "INSERT INTO api_cache(plugin_id, query_hash, params_json, fetched_at, ttl_s, payload) "
                      "VALUES(?,?,?,?,?,jsonb(?)) "
                      "ON CONFLICT(plugin_id, query_hash) DO UPDATE SET "
                      "params_json=excluded.params_json, fetched_at=excluded.fetched_at, "
                      "ttl_s=excluded.ttl_s, payload=excluded.payload";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, plugin_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, query_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, params_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, fetched_at);
        sqlite3_bind_int64(stmt, 5, ttl_s);
        sqlite3_bind_text(stmt, 6, payload.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt); // fire-and-forget write; failure = cache miss next time
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

std::optional<SavedManga> Store::saved_get(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
        where(c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty())
        return std::nullopt;
    const auto& r = rows[0];
    return SavedManga{r.plugin, r.slug, r.name, r.cover, r.added_at};
}
