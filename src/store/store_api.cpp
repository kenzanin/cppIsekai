#include "store.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;

std::optional<ApiCacheRow> Store::api_cache_get(const std::string& plugin_id, const std::string& query_hash) {
    // Raw connection: payload is JSONB binary — sqlite_orm has no mapping for it.
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK)
        return std::nullopt;
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

void Store::api_cache_put(const std::string& plugin_id, const std::string& query_hash, const std::string& params_json,
                          const std::string& payload, std::int64_t fetched_at, std::int64_t ttl_s) {
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK)
        return;
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
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty())
        return std::nullopt;
    const auto& r = rows[0];
    return SavedManga{r.plugin,  r.slug,       r.name,         r.cover,      r.added_at, r.status,     r.total_ch,
                      r.read_ch, r.has_update, r.last_read_at, r.updated_at, r.synopsis, r.alt_titles, r.dup_group};
}

void Store::upsert_update_attempt(const std::string& plugin, const std::string& slug, const std::string& outcome,
                                  const std::string& error) {
    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    auto rows = impl_->storage.get_all<store_detail::UpdateAttemptsEntity>(
        where(c(&store_detail::UpdateAttemptsEntity::plugin) == plugin &&
              c(&store_detail::UpdateAttemptsEntity::slug) == slug));
    store_detail::UpdateAttemptsEntity e{};
    e.plugin = plugin;
    e.slug = slug;
    e.last_attempt_at = now;
    e.outcome = outcome;
    e.error = error;
    if (outcome == "ok") {
        e.consecutive_errors = 0;
        e.backoff_until = 0;
    } else {
        e.consecutive_errors = (rows.empty() ? 0 : rows[0].consecutive_errors) + 1;
        // min(2^n, 72) hours of backoff; 429 is classified upstream and flows through the same math
        long long hours = (e.consecutive_errors >= 7) ? 72 : (1LL << e.consecutive_errors);
        if (hours > 72) hours = 72;
        e.backoff_until = now + hours * 3600;
    }
    impl_->storage.replace(e);
}

std::optional<UpdateAttempt> Store::update_attempt_get(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::UpdateAttemptsEntity>(
        where(c(&store_detail::UpdateAttemptsEntity::plugin) == plugin &&
              c(&store_detail::UpdateAttemptsEntity::slug) == slug));
    if (rows.empty())
        return std::nullopt;
    const auto& r = rows[0];
    return UpdateAttempt{r.plugin, r.slug, r.last_attempt_at, r.outcome, r.error, r.consecutive_errors, r.backoff_until};
}

std::vector<UpdateAttempt> Store::update_candidates(std::int64_t now, int per_plugin_limit) {
    // Window function: rank per plugin, keep the first N by never-attempted-first then oldest last_attempt_at.
    // read_ch <= 1: barely-read titles only. Backed-off rows are excluded.
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK)
        return {};
    sqlite3_busy_timeout(db, 3000);
    const char* sql =
        "SELECT plugin, slug, COALESCE(last_attempt_at,0), COALESCE(outcome,''), COALESCE(error,''), "
        "       COALESCE(consecutive_errors,0), COALESCE(backoff_until,0) "
        "FROM (SELECT s.plugin AS plugin, s.slug AS slug, a.last_attempt_at, a.outcome, a.error, "
        "             a.consecutive_errors, a.backoff_until, "
        "             ROW_NUMBER() OVER (PARTITION BY s.plugin "
        "                 ORDER BY (a.last_attempt_at IS NULL) DESC, a.last_attempt_at ASC) AS rn "
        "      FROM saved_manga s LEFT JOIN update_attempts a "
        "           ON a.plugin = s.plugin AND a.slug = s.slug "
        "      WHERE s.read_ch <= 1 AND (a.backoff_until IS NULL OR a.backoff_until <= ?)) "
        "WHERE rn <= ? ORDER BY plugin, (last_attempt_at IS NULL) DESC, last_attempt_at ASC";
    sqlite3_stmt* stmt = nullptr;
    std::vector<UpdateAttempt> out;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now);
        sqlite3_bind_int(stmt, 2, per_plugin_limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UpdateAttempt a;
            a.plugin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            a.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            a.last_attempt_at = sqlite3_column_int64(stmt, 2);
            a.outcome = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            a.error = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            a.consecutive_errors = sqlite3_column_int(stmt, 5);
            a.backoff_until = sqlite3_column_int64(stmt, 6);
            out.push_back(std::move(a));
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return out;
}
