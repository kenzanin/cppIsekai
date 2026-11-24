#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;


void Store::upsert_update_attempt(const std::string& plugin, const std::string& slug, const std::string& outcome,
                                  const std::string& error) {
    const store_detail::OpTrace _op{"upsert_update_attempt"};
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
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
        hours = std::min<long long>(hours, 72);
        e.backoff_until = now + (hours * 3600);
    }
    impl_->storage.replace(e);
}

std::optional<UpdateAttempt> Store::update_attempt_get(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"update_attempt_get"};
    auto rows = impl_->storage.get_all<store_detail::UpdateAttemptsEntity>(
        where(c(&store_detail::UpdateAttemptsEntity::plugin) == plugin &&
              c(&store_detail::UpdateAttemptsEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return UpdateAttempt{
        .plugin = r.plugin,
        .slug = r.slug,
        .last_attempt_at = r.last_attempt_at,
        .outcome = r.outcome,
        .error = r.error,
        .consecutive_errors = r.consecutive_errors,
        .backoff_until = r.backoff_until,
    };
}

std::vector<UpdateAttempt> Store::update_candidates(std::int64_t now, int per_plugin_limit) {
    const store_detail::OpTrace _op{"update_candidates"};
    // Window function: rank per plugin, keep the first N by never-attempted-first then oldest last_attempt_at.
    // read_ch <= 1: barely-read titles only. Backed-off rows are excluded.
    sqlite3* db = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) {
        return {};
    }
    store_detail::attach_busy(db);
    const char* sql = "SELECT plugin, slug, COALESCE(last_attempt_at,0), COALESCE(outcome,''), COALESCE(error,''), "
                      "       COALESCE(consecutive_errors,0), COALESCE(backoff_until,0) "
                      "FROM (SELECT s.plugin AS plugin, s.slug AS slug, a.last_attempt_at, a.outcome, a.error, "
                      "             a.consecutive_errors, a.backoff_until, "
                      "             ROW_NUMBER() OVER (PARTITION BY s.plugin "
                      "                 ORDER BY (a.last_attempt_at IS NULL) DESC, a.last_attempt_at ASC) AS rn "
                      "      FROM saved_manga s LEFT JOIN update_attempts a "
                      "           ON a.plugin = s.plugin AND a.slug = s.slug "
                      "      WHERE s.read_ch <= 1 AND (a.backoff_until IS NULL OR a.backoff_until <= ?) "
                      "        AND EXISTS(SELECT 1 FROM plugin p WHERE p.plugin_id = s.plugin AND p.enabled = 1)) "
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

std::vector<std::string> Store::plugin_prune_absent(const std::vector<std::string>& keep_ids) {
    const store_detail::OpTrace _op{"plugin_prune_absent"};
    // folder-removal GC (plugin-lifecycle 2.1): one transaction per gone
    // plugin, children first -- title hangs off the registry row id, the rest
    // key on the string plugin id. The plugin row dies last.
    sqlite3* db = nullptr;
    std::vector<std::string> removed;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) {
        return removed;
    }
    store_detail::attach_busy(db);
    std::vector<std::string> all;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT plugin_id FROM plugin WHERE plugin_id <> ''", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            all.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    // library-ownership 3.1: hosting state only. Library data (saved_manga,
    // chapters, chapter_pages, reader_positions, title_meta, update_attempts)
    // survives as orphans -- a reinstall with the same id revives it.
    const char* const cascade[] = {
        "DELETE FROM plugin_state WHERE plugin_id = ?",
        "DELETE FROM api_cache WHERE plugin_id = ?",
        "DELETE FROM plugin WHERE plugin_id = ?",
    };
    for (const auto& id : all) {
        if (std::ranges::find(keep_ids, id) != keep_ids.end()) {
            continue;
        }
        sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);
        bool ok = true;
        for (const char* sql : cascade) {
            sqlite3_stmt* del = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &del, nullptr) == SQLITE_OK &&
                sqlite3_bind_text(del, 1, id.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK) {
                ok = sqlite3_step(del) == SQLITE_DONE && ok;
            } else {
                ok = false;
            }
            sqlite3_finalize(del);
        }
        sqlite3_exec(db, ok ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
        if (ok) {
            removed.push_back(id);
        }
    }
    sqlite3_close(db);
    return removed;
}
