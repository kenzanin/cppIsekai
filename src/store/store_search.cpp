#include "store.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <map>
#include <vector>
#include <sqlite3.h>
#include <utf8proc.h>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;

namespace {
// library-dedup 1.4: keep library_fts in sync. Delete+insert is the spec's
// "same transaction" requirement in effect: save path is synchronous on the
// db thread, one writer (db_pool=1). tsv-style doubled column = FTS5 escape.
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
    sqlite3_busy_timeout(db, 3000);
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
    sqlite3_busy_timeout(db, 3000);
    sqlite3_stmt* del = nullptr;
    if (sqlite3_prepare_v2(db, "DELETE FROM library_fts WHERE plugin=? AND slug=?", -1, &del, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(del, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(del, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }
    sqlite3_close(db);
}
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
}

} // namespace

void Store::recompute_dup_groups() {
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
        sqlite3_busy_timeout(db, 3000);
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
    sqlite3_busy_timeout(db, 3000);
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

std::vector<Plugin> Store::plugins() {
    auto rows = impl_->storage.select(
        columns(&store_detail::PluginEntity::id, &store_detail::PluginEntity::name,
                sqlite_orm::count(&store_detail::TitleEntity::id), &store_detail::PluginEntity::plugin_id,
                &store_detail::PluginEntity::version, &store_detail::PluginEntity::enabled,
                &store_detail::PluginEntity::runtime, &store_detail::PluginEntity::capabilities),
        left_join<store_detail::TitleEntity>(
            on(c(&store_detail::TitleEntity::plugin_id) == &store_detail::PluginEntity::id)),
        group_by(&store_detail::PluginEntity::id), order_by(&store_detail::PluginEntity::name));
    std::vector<Plugin> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        Plugin p;
        p.id = std::get<0>(row);
        p.name = std::get<1>(row);
        p.titles = std::get<2>(row);
        p.plugin_id = std::get<3>(row);
        p.version = std::get<4>(row);
        p.enabled = std::get<5>(row) != 0;
        p.runtime = std::get<6>(row);
        p.caps = std::get<7>(row);
        out.push_back(std::move(p));
    }
    return out;
}

void Store::upsert_plugin(const std::string& id, const std::string& name, const std::string& version,
                          const std::string& capabilities, const std::string& runtime) {
    auto rows =
        impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::plugin_id) == id));
    if (!rows.empty()) {
        rows[0].name = name;
        rows[0].version = version;
        rows[0].capabilities = capabilities;
        rows[0].runtime = runtime;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(store_detail::PluginEntity{.id = 0,
                                                     .name = name,
                                                     .plugin_id = id,
                                                     .version = version,
                                                     .capabilities = capabilities,
                                                     .enabled = 1,
                                                     .runtime = runtime});
}

void Store::set_plugin_enabled(std::int64_t id, bool on) {
    impl_->storage.update_all(set(c(&store_detail::PluginEntity::enabled) = on ? 1 : 0),
                              where(c(&store_detail::PluginEntity::id) == id));
}

bool Store::plugin_enabled(const std::string& plugin_id) {
    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(
        where(c(&store_detail::PluginEntity::plugin_id) == plugin_id));
    if (rows.empty()) {
        return true; // unregistered id: nothing to gate
    }
    return rows[0].enabled != 0;
}

namespace {
// LIKE pattern with %, _ and \\ escaped so user query stays literal; pair with .escape("\\").
std::string like_pattern(const std::string& query) {
    std::string out = "%";
    for (char ch : query) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            out += '\\';
        }
        out += ch;
    }
    return out + "%";
}
} // namespace

int Store::count(const std::string& plugin, const std::string& query) {
    const auto pattern = like_pattern(query);
    if (plugin.empty()) {
        return impl_->storage.count<store_detail::TitleEntity>(
            where(like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\"))));
    }

    auto rows =
        impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::name) == plugin));
    if (rows.empty()) {
        return 0;
    }
    return impl_->storage.count<store_detail::TitleEntity>(
        where(c(&store_detail::TitleEntity::plugin_id) == rows[0].id &&
              like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\"))));
}

std::vector<Title> Store::search(const std::string& plugin, const std::string& query, int limit, int offset) {
    const auto pattern = like_pattern(query);
    // LIKE '%' on plugin matches every row: one condition shape, no empty-branch special case.
    auto rows = impl_->storage.select(
        columns(&store_detail::TitleEntity::id, &store_detail::TitleEntity::name, &store_detail::PluginEntity::name),
        inner_join<store_detail::PluginEntity>(
            on(c(&store_detail::PluginEntity::id) == &store_detail::TitleEntity::plugin_id)),
        where(like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\")) &&
              like(&store_detail::PluginEntity::name, plugin.empty() ? "%" : plugin)),
        order_by(&store_detail::TitleEntity::name),
        // v1.9.1: standalone offset() serializer is incomplete; the limit(lim, offset) overload is the supported form.
        sqlite_orm::limit(limit, sqlite_orm::offset(offset)));
    std::vector<Title> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        Title t;
        t.id = std::get<0>(row);
        t.name = std::get<1>(row);
        t.plugin = std::get<2>(row);
        out.push_back(std::move(t));
    }
    return out;
}

std::optional<std::string> Store::state_get(const std::string& plugin_id, const std::string& key) {
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id && c(&store_detail::StateEntity::key) == key));
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows[0].value;
}

void Store::state_set(const std::string& plugin_id, const std::string& key, const std::string& value) {
    impl_->storage.replace(store_detail::StateEntity{
        .plugin_id = plugin_id, .key = key, .value = value}); // INSERT OR REPLACE on the composite pk: upsert
}

std::vector<std::string> Store::state_keys_value(const std::string& plugin_id, const std::string& key_prefix,
                                                 const std::string& value) {
    std::string pat;
    pat.reserve(key_prefix.size() + 1);
    for (char ch : key_prefix) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            pat += '\\';
        }
        pat += ch;
    }
    pat += '%';
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id &&
              like(&store_detail::StateEntity::key, pat).escape(std::string("\\")) &&
              c(&store_detail::StateEntity::value) == value));
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(r.key);
    }
    return out;
}

void Store::position_set(const ReaderPosition& pos) {
    // REPLACE on the (plugin, slug) PK: the route layers optional fields over
    // the stored row, this just persists the result. updated_at stamped here.
    impl_->storage.replace(store_detail::ReaderPositionEntity{
        .plugin = pos.plugin,
        .slug = pos.slug,
        .chapter_id = pos.chapter_id,
        .chapter_number = pos.chapter_number,
        .page = pos.page,
        .mode = pos.mode,
        .direction = pos.direction,
        .zoom = pos.zoom,
        .updated_at = static_cast<std::int64_t>(std::time(nullptr)),
    });
}

std::optional<ReaderPosition> Store::position_get(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == plugin &&
              c(&store_detail::ReaderPositionEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return ReaderPosition{
        .plugin = r.plugin,
        .slug = r.slug,
        .chapter_id = r.chapter_id,
        .chapter_number = r.chapter_number,
        .page = r.page,
        .mode = r.mode,
        .direction = r.direction,
        .zoom = r.zoom,
        .updated_at = r.updated_at,
    };
}

std::optional<ReaderPosition> Store::position_last() {
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        order_by(&store_detail::ReaderPositionEntity::updated_at).desc(), limit(1));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return ReaderPosition{
        .plugin = r.plugin,
        .slug = r.slug,
        .chapter_id = r.chapter_id,
        .chapter_number = r.chapter_number,
        .page = r.page,
        .mode = r.mode,
        .direction = r.direction,
        .zoom = r.zoom,
        .updated_at = r.updated_at,
    };
}

static store_detail::SavedMangaEntity saved_row(const SavedManga& m, std::int64_t added_at) {
    return store_detail::SavedMangaEntity{
        .plugin = m.plugin,
        .slug = m.slug,
        .name = m.name,
        .cover = m.cover,
        .added_at = added_at,
        .status = m.status.empty() ? std::string{"ongoing"} : m.status,
        .total_ch = m.total_ch,
        .read_ch = m.read_ch,
        .has_update = m.has_update,
        .last_read_at = m.last_read_at,
        .updated_at = m.updated_at,
        .synopsis = m.synopsis,
        .alt_titles = m.alt_titles,
        .dup_group = m.dup_group,
    };
}

void Store::save_manga(const SavedManga& m) {
    // REPLACE on the (plugin, slug) PK: refreshes name/cover/added_at on re-save.
    // Preserve any existing stat columns so a plain re-save never resets progress.
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == m.plugin && c(&store_detail::SavedMangaEntity::slug) == m.slug));
    store_detail::SavedMangaEntity e = saved_row(m, static_cast<std::int64_t>(std::time(nullptr)));
    if (!rows.empty()) {
        e.status = rows[0].status;
        e.total_ch = rows[0].total_ch;
        e.read_ch = rows[0].read_ch;
        e.has_update = rows[0].has_update;
        e.last_read_at = rows[0].last_read_at;
        e.updated_at = rows[0].updated_at;
        // library-dedup 1.2: preserve signal columns (envelope sync owns them)
        e.synopsis = rows[0].synopsis;
        e.alt_titles = rows[0].alt_titles;
        e.dup_group = rows[0].dup_group;
    }
    impl_->storage.replace(e);
    fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis); // library-dedup 1.4
}

void Store::saved_stats_update(const std::string& plugin, const std::string& slug, const std::string& status,
                               int total_ch) {
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    const bool changed = (!status.empty() && e.status != status) || e.total_ch != total_ch;
    if (!status.empty()) {
        e.status = status;
    }
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    if (total_ch > e.total_ch) {
        e.has_update = 1; // growth → mark New
    }
    e.total_ch = total_ch;
    if (changed) {
        e.updated_at = now;
    }
    // FTS NOT re-synced here (library-dedup 1.4): stats path touches no indexed column (name/alt_titles/synopsis).
    impl_->storage.replace(e);
}

void Store::saved_signal_update(const std::string& plugin, const std::string& slug, const std::string& synopsis,
                                const std::string& alt_titles_json) {
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    bool changed = false;
    if (!synopsis.empty() && e.synopsis != synopsis) {
        e.synopsis = synopsis;
        changed = true;
    }
    if (!alt_titles_json.empty() && e.alt_titles != alt_titles_json) {
        e.alt_titles = alt_titles_json;
        changed = true;
    }
    if (changed) {
        e.updated_at = static_cast<std::int64_t>(std::time(nullptr));
        impl_->storage.replace(e);
        fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis); // library-dedup 1.4
    }
}

void Store::saved_read_rollup(const std::string& plugin, const std::string& slug, int read_ch,
                              std::int64_t last_read_at) {
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    e.read_ch = read_ch;
    if (last_read_at > 0) {
        e.last_read_at = last_read_at;
    }
    impl_->storage.replace(e);
}

void Store::saved_clear_update(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    e.last_read_at = now; // reader touch (library-grid-stats 2.4), flag or not
    if (e.has_update != 0) {
        e.has_update = 0;
        e.updated_at = now;
    }
    impl_->storage.replace(e);
}

void Store::unsave_manga(const std::string& plugin, const std::string& slug) {
    impl_->storage.remove_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    fts_delete(*impl_, plugin, slug); // library-dedup 1.4
}

std::vector<SavedManga> Store::saved_list() {
    // added_at has second resolution, and REPLACE re-stamps it on re-save, so
    // newest-first by added_at alone is stable enough for the single-user grid.
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
        order_by(&store_detail::SavedMangaEntity::added_at).desc());
    std::vector<SavedManga> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(SavedManga{
            .plugin = r.plugin,
            .slug = r.slug,
            .name = r.name,
            .cover = r.cover,
            .added_at = r.added_at,
            .status = r.status,
            .total_ch = r.total_ch,
            .read_ch = r.read_ch,
            .has_update = r.has_update,
            .last_read_at = r.last_read_at,
            .updated_at = r.updated_at,
            .synopsis = r.synopsis,
            .alt_titles = r.alt_titles,
            .dup_group = r.dup_group,
        });
    }
    return out;
}
