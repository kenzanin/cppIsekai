#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;

std::optional<ApiCacheRow> Store::api_cache_get(const std::string& plugin_id, const std::string& query_hash) {
    const store_detail::OpTrace _op{"api_cache_get"};
    // Raw (JSONB) handle: opened once per process, guarded by OpTrace's
    // raw_db_mutex -- sqlite_orm has no mapping for the binary payload.
    sqlite3* db = impl_->raw_db();
    if (db == nullptr) {
        return std::nullopt;
    }
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
            if ((payload != nullptr) && plen > 0) {
                row = ApiCacheRow{
                    .params_json = (params != nullptr) ? params : "",
                    .payload = std::string(payload, static_cast<std::size_t>(plen)),
                    .fetched_at = sqlite3_column_int64(stmt, 2),
                    .ttl_s = sqlite3_column_int64(stmt, 3),
                };
            }
        }
        sqlite3_finalize(stmt);
    }
    return row;
}

void Store::api_cache_put(const std::string& plugin_id, const std::string& query_hash, const std::string& params_json,
                          const std::string& payload, std::int64_t fetched_at, std::int64_t ttl_s) {
    const store_detail::OpTrace _op{"api_cache_put"};
    sqlite3* db = impl_->raw_db();
    if (db == nullptr) {
        return;
    }
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
}

int Store::api_cache_clear(const std::string& plugin_id) {
    const store_detail::OpTrace _op{"api_cache_clear"};
    // Empty id clears every plugin; else just that plugin's envelopes.
    sqlite3* db = impl_->raw_db();
    if (db == nullptr) {
        return -1;
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql = plugin_id.empty() ? "DELETE FROM api_cache" : "DELETE FROM api_cache WHERE plugin_id = ?";
    int changes = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (!plugin_id.empty()) {
            sqlite3_bind_text(stmt, 1, plugin_id.c_str(), -1, SQLITE_TRANSIENT);
        }
        sqlite3_step(stmt);
        changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }
    return changes;
}

void Store::saved_set_pinned(const std::string& plugin, const std::string& slug, bool pinned) {
    const store_detail::OpTrace _op{"saved_set_pinned"};
    impl_->storage.update_all(
        set(c(&store_detail::SavedMangaEntity::pinned) = pinned ? 1 : 0),
        where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
              c(&store_detail::SavedMangaEntity::slug) == slug));
}

std::optional<SavedManga> Store::saved_get(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"saved_get"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return SavedManga{
        .plugin = r.plugin,
        .slug = r.slug,
        .name = r.name,
        .cover = r.cover,
        .added_at = r.added_at,
        .status = r.status,
        .total_ch = r.total_ch,
        .read_ch = r.read_ch,
        .last_read_at = r.last_read_at,
        .updated_at = r.updated_at,
        .synopsis = r.synopsis,
        .alt_titles = r.alt_titles,
        .dup_group = r.dup_group,
        .status_flags = r.status_flags,
        // manga-detail-table: materialized display detail (empty until first write)
        .authors = r.authors,
        .artists = r.artists,
        .tags = r.tags,
        .synopsis_display = r.synopsis_display,
        .pinned = r.pinned,
    };
}

