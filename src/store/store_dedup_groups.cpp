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

#include "store_dedup_internal.hpp"

using namespace sqlite_orm;

void Store::recompute_dup_groups() {
    const store_detail::OpTrace _op{"recompute_dup_groups"};
    // library-dedup 2.2 (design D3): FTS5 MATCH candidates, bm25-ordered, Dice
    // similarity >= 0.6, union-find components. All writes in ONE txn.
    auto rows =
        impl_->storage.get_all<store_detail::SavedMangaEntity>(order_by(&store_detail::SavedMangaEntity::added_at));
    if (rows.empty()) {
        return;
    }
    if (!impl_->fts5_ok || rows.size() < 2) {
        // degraded mode: everything unique, one txn clears any stale values
        sqlite3* db = nullptr;
        if (sqlite3_open_v2(impl_->path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
            if (db != nullptr) {
                sqlite3_close(db);
            }
            return;
        }
        store_detail::attach_busy(db);
        sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        for (const auto& r : rows) {
            if (r.dup_group != 0) {
                sqlite3_stmt* st = nullptr;
                if (sqlite3_prepare_v2(db, "UPDATE saved_manga SET dup_group=0 WHERE plugin=? AND slug=?", -1, &st,
                                       nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(st, 1, r.plugin.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(st, 2, r.slug.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(st);
                    sqlite3_finalize(st);
                }
            }
        }
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return;
    }
    const std::size_t n = rows.size();
    std::vector<store_detail::TokenMultiset> titles(n);
    std::vector<store_detail::TokenMultiset> names(n);
    std::vector<store_detail::TokenMultiset> syms(n);
    for (std::size_t i = 0; i < n; ++i) {
        titles[i] = store_detail::tokenize(rows[i].name + " " + rows[i].alt_titles);
        names[i] = store_detail::tokenize(rows[i].name);
        syms[i] = store_detail::tokenize(rows[i].synopsis);
    }
    // union-find over indices
    std::vector<int> parent(n);
    for (std::size_t i = 0; i < n; ++i) {
        parent[i] = static_cast<int>(i);
    }
    auto find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    auto unite = [&](int a, int b) {
        a = find(a);
        b = find(b);
        if (a != b) {
            parent[a] = b;
        }
    };
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(impl_->path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return;
    }
    store_detail::attach_busy(db);
    for (std::size_t i = 0; i < n; ++i) {
        // candidate set: title tokens, OR-joined (alnum so MATCH-safe); skip pure-stopword noise
        std::string match;
        for (const auto& [tok, _] : titles[i]) {
            if (!match.empty()) {
                match += " OR ";
            }
            match += tok;
            if (match.size() > 400) {
                break; // cap the query; MATCH accepts longer but keep it cheap
            }
        }
        if (match.empty()) {
            continue;
        }
        const std::string sql =
            "SELECT plugin, slug, bm25(library_fts) FROM library_fts WHERE library_fts MATCH '{name alt_titles} : (" +
            match + ")' ORDER BY bm25(library_fts) LIMIT 20";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
            continue;
        }
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* cp = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            const char* cs = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            if ((cp == nullptr) || (cs == nullptr)) {
                continue;
            }
            for (std::size_t j = 0; j < n; ++j) {
                if (j == i) {
                    continue;
                }
                if (rows[j].plugin != cp || rows[j].slug != cs) {
                    continue;
                }
                const double td = store_detail::dice(titles[i], titles[j]);
                const double sd = store_detail::dice(syms[i], syms[j]);
                // name-only dice: alt_titles dilute one side's multiset (64 vs 9
                // tokens => 0.247 for identical names); the bare name is the
                // strongest single signal. Standalone paths fire only near
                // identical (0.75): generic overlaps ('... middle aged man ...
                // isekai ...') reach 0.6+ on shared phrases alone (observed
                // 0.667 for two different series); below that the synopsis must
                // corroborate via the mixed term.
                const double nd = store_detail::dice(names[i], names[j]);
                // synopsis Dice counts double-ish as corroboration (design D3)
                const double sim = std::max({td >= 0.75 ? td : 0.0, (0.5 * td) + (0.5 * sd), nd >= 0.75 ? nd : 0.0});
                if (sim >= 0.6) {
                    unite(static_cast<int>(i), static_cast<int>(j));
                }
            }
        }
        sqlite3_finalize(st);
    }
    // assign group ids: singletons -> 0; components of size >= 2 -> sequential ids starting at 1
    std::map<int, int> comp_id;
    std::vector<int> out(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        int root = find(static_cast<int>(i));
        auto it = comp_id.find(root);
        if (it != comp_id.end()) {
            out[i] = it->second;
        // provisional marker: decide after counting component sizes
        } else {
            comp_id[root] = -1;
        }
    }
    std::map<int, int> counts;
    for (std::size_t i = 0; i < n; ++i) {
        ++counts[find(static_cast<int>(i))];
    }
    int gid = 0;
    comp_id.clear();
    for (std::size_t i = 0; i < n; ++i) {
        int root = find(static_cast<int>(i));
        if (counts[root] < 2) {
            out[i] = 0;
            continue;
        }
        auto it = comp_id.find(root);
        if (it == comp_id.end()) {
            it = comp_id.emplace(root, ++gid).first;
        }
        out[i] = it->second;
    }
    // write all rows in one txn (singletons too, so stale groups clear)
    sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    for (std::size_t i = 0; i < n; ++i) {
        if (rows[i].dup_group == out[i]) {
            continue;
        }
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE saved_manga SET dup_group=? WHERE plugin=? AND slug=?", -1, &st, nullptr) ==
            SQLITE_OK) {
            sqlite3_bind_int(st, 1, out[i]);
            sqlite3_bind_text(st, 2, rows[i].plugin.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 3, rows[i].slug.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
    }
    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}
