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

namespace {
// --- library-dedup 2.1 tokeniser (design D5) -----------------------
// utf8proc pipeline: NFKD + casefold + strip combining marks; per-cp
// katakana->hiragana fold plus a tiny no-decomposition letter table.
// (ponytail upgrade of the hand-rolled Latin tables: full-Unicode
// folding without ICU. Tokens are runtime-only: dup groups recompute
// from titles on the next pass, no stored-token migration.)
bool is_separator(uint32_t cp) {
    if (cp < 0x21) {
        return true; // C0 controls + space
    }
    if (cp >= 0x2000 && cp <= 0x206F) {
        return true; // general punctuation
    }
    if (cp >= 0x2E00 && cp <= 0x2E7F) {
        return true;
    }
    if (cp >= 0x3000 && cp <= 0x303F) {
        return true; // CJK symbols & punctuation
    }
    if (cp == 0xFF01) {
        return true;
    }
    return cp == 0xFF0C || cp == 0xFF0E || cp == 0xFF1A || cp == 0xFF1B || cp == 0xFF08 || cp == 0xFF09 ||
           cp == 0xFF5B || cp == 0xFF5D;
}
// Letters utf8proc NFKD leaves intact (no decomposition) -> ASCII base,
// mirroring the old fold table's coverage.
uint32_t fold_residual(uint32_t cp) {
    switch (cp) {
        case 0x00F8: return 'o'; // ø
        case 0x00E6: return 'a'; // æ
        case 0x0111: return 'd'; // đ
        case 0x0131: return 'i'; // ı
        case 0x0142: return 'l'; // ł
        case 0x014A: return 'n'; // ŋ
        case 0x0153: return 'o'; // œ
        case 0x00FE: return 't'; // þ
        default: return cp;
    }
}
void encode_utf8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
using TokenMultiset = std::map<std::string, int>;
TokenMultiset tokenize(const std::string& s) {
    TokenMultiset out;
    std::string cur;
    auto flush = [&] {
        if (cur.empty()) {
            return;
        }
        out[std::move(cur)] += 1;
        cur.clear();
    };
    const utf8proc_ssize_t max_cp = static_cast<utf8proc_ssize_t>(s.size()) * 4 + 16;
    std::vector<utf8proc_int32_t> buffer(max_cp);
    const utf8proc_ssize_t ncp = utf8proc_decompose(
        reinterpret_cast<const utf8proc_uint8_t*>(s.data()),
        static_cast<utf8proc_ssize_t>(s.size()),
        buffer.data(), max_cp,
        static_cast<utf8proc_option_t>(UTF8PROC_COMPAT | UTF8PROC_DECOMPOSE | UTF8PROC_CASEFOLD | UTF8PROC_STRIPMARK));
    if (ncp < 0) {
        return out; // undecomposable input: no tokens
    }
    for (utf8proc_ssize_t i = 0; i < ncp; ++i) {
        const uint32_t cp = static_cast<uint32_t>(buffer[i]);
        if (is_separator(cp)) {
            flush();
            continue;
        }
        uint32_t folded = fold_residual(cp);
        if (folded >= 0x30A1 && folded <= 0x30F6) {
            folded -= 0x60; // katakana -> hiragana
        }
        if ((folded >= 'a' && folded <= 'z') || (folded >= '0' && folded <= '9')) {
            cur.push_back(static_cast<char>(folded));
        } else if (folded >= 0x1100) {
            encode_utf8(folded, cur); // CJK/kana/etc: verbatim
        } else {
            flush();
        }
    }
    flush();
    return out;
}
// Dice coefficient over two multisets: 2*Σmin(a,b) / (Σa + Σb)
double dice(const TokenMultiset& a, const TokenMultiset& b) {
    if (a.empty() || b.empty()) {
        return 0.0;
    }
    long sum = 0;
    long inter = 0;
    for (const auto& [k, v] : a) {
        sum += v;
    }
    for (const auto& [k, v] : b) {
        sum += v;
        auto it = a.find(k);
        if (it != a.end()) {
            inter += std::min(static_cast<long>(v), static_cast<long>(it->second));
        }
    }
    return (sum == 0) ? 0.0 : (2.0 * static_cast<double>(inter)) / static_cast<double>(sum);
    return (sum == 0) ? 0.0 : (2.0 * static_cast<double>(inter)) / static_cast<double>(sum);
}

} // namespace

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
    std::vector<TokenMultiset> titles(n);
    std::vector<TokenMultiset> syms(n);
    for (std::size_t i = 0; i < n; ++i) {
        titles[i] = tokenize(rows[i].name + " " + rows[i].alt_titles);
        syms[i] = tokenize(rows[i].synopsis);
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
                const double td = dice(titles[i], titles[j]);
                const double sd = dice(syms[i], syms[j]);
                // synopsis Dice counts double-ish as corroboration (design D3)
                const double sim = std::max(td, (0.5 * td) + (0.5 * sd));
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
