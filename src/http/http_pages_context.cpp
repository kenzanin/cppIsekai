#include "enrich_internal.hpp"
#include "http_routes.hpp"

#include "plugin/url_codec.hpp"

#include <glaze/glaze.hpp>
#include <optional>
#include <utility>

#include <fstream>
#include <iterator>
#include <sstream>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace http {


// library-migrate: normalized title key (alnum, lowercased) for the exact-match
// badge; the corpus is the stored name + every alt title, precomputed here.
std::string title_key(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u) != 0) {
            out += static_cast<char>(std::tolower(u));
        }
    }
    return out;
}

crow::json::wvalue layout(Store& /*store*/, BS::thread_pool<>& /*db*/, const char* active) {
    crow::json::wvalue ctx;
    ctx["active_" + std::string(active)] = true;
    return ctx;
}

crow::json::wvalue home_context(Store& store, BS::thread_pool<>& db, const crow::request& req,
                                reader::CoverWorker& covers) {
    const std::string plugin = param(req, "plugin");
    const std::string q = param(req, "q");
    const int page_no = page_of(param(req, "page"));

    const auto plugins = on_pool(db, [&] { return store.plugins(); });

    auto ctx = layout(store, db, "library");
    ctx["q"] = q;
    ctx["plugin"] = plugin;
    ctx["filtered"] = !plugin.empty();

    // Sidebar counts the user's library (saved_manga), not the legacy title
    // search-cache, which is empty for plugin-supplied results.
    std::vector<SavedManga> saved = on_pool(db, [&] { return store.saved_list(); });
    std::map<std::string, int> saved_by_plugin;
    for (const auto& m : saved) {
        ++saved_by_plugin[m.plugin];
    }

    // Most-used plugin first: sort sidebar rows by saved-library count (desc, stable = registry order within ties).
    std::vector<std::pair<int, crow::json::wvalue>> counted_rows;
    counted_rows.reserve(plugins.size());
    for (const auto& item : plugins) {
        // plugin-lifecycle: disabled stays listed in the library sidebar —
        // only the search picker drops it (search_script.mustache p.enabled).
        crow::json::wvalue row;
        row["name"] = item.name;
        row["id"] = item.plugin_id; // client grid filter + ?plugin= deep-link key off plugin_id
        row["titles"] = std::to_string(saved_by_plugin[item.plugin_id]); // saved_manga is the Library; title
                                                                         // search-cache is empty for plugin results
        row["current"] = item.plugin_id == plugin;
        if (!item.plugin_id.empty()) {
            row["logo"] = item.plugin_id; // absent key renders no <img> in the partial
        }
        counted_rows.emplace_back(saved_by_plugin[item.plugin_id], std::move(row));
    }
    std::stable_sort(counted_rows.begin(), counted_rows.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<crow::json::wvalue> plugin_rows;
    plugin_rows.reserve(counted_rows.size());
    for (auto& p : counted_rows) {
        plugin_rows.push_back(std::move(p.second));
    }
    ctx["plugins"] = std::move(plugin_rows);

    // library-save 4.1: saved entries are the Library — when any are saved,
    // the grid renders them (added_at desc) instead of the search-cache titles.
    const int saved_total_all = static_cast<int>(saved.size());
    const int dup_total = static_cast<int>(std::count_if(saved.begin(), saved.end(), [](const SavedManga& m) { return m.dup_group > 0; }));
    // Cover prefetch: hand the saved covers to the cover worker; cache-first,
    // so entries already on disk are not re-fetched.
    // overlay chosen enrichment covers/titles so the grid matches the detail
    // page: ONE batched title_meta read + a single parse pass (hot-path-perf P1)
    // instead of 2N mutexed per-title queries.
    std::vector<std::pair<std::string, std::string>> tm_keys;
    tm_keys.reserve(saved.size());
    for (const auto& m : saved) {
        tm_keys.emplace_back(m.plugin, m.slug);
    }
    const auto tm = on_pool(db, [&] { return store.title_meta_get_many(tm_keys); });
    const auto chosen_field = [](const glz::generic& chosen, const char* key) -> std::optional<std::string> {
        if (!chosen.is_object()) {
            return std::nullopt;
        }
        const auto& cd = chosen.get_object();
        const auto it = cd.find(key);
        if (it == cd.end()) {
            return std::nullopt;
        }
        if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
            return *s;
        }
        return std::nullopt;
    };
    std::vector<std::optional<std::string>> ovl(saved.size());
    std::vector<std::optional<std::string>> ovl_title(saved.size());
    for (std::size_t i = 0; i < saved.size(); ++i) {
        const auto it = tm.find({saved[i].plugin, saved[i].slug});
        if (it == tm.end()) {
            continue;
        }
        glz::generic chosen;
        if (glz::read_json(chosen, it->second.chosen) != glz::error_code::none) {
            continue;
        }
        ovl[i] = chosen_field(chosen, "cover");
        ovl_title[i] = chosen_field(chosen, "title");
    }
    std::vector<reader::CoverJob> jobs;
    for (std::size_t i = 0; i < saved.size(); ++i) {
        const std::string& c = ovl[i].has_value() ? *ovl[i] : saved[i].cover;
        if (!c.empty()) {
            jobs.push_back({.url = c, .plugin = saved[i].plugin});
        }
    }
    if (!jobs.empty()) {
        covers.nudge(std::move(jobs));
    }
    ctx["saved_mode"] = !saved.empty();
    ctx["empty_saved"] = saved.empty();
    std::vector<crow::json::wvalue> saved_rows;
    for (std::size_t i = 0; i < saved.size(); ++i) {
        const auto& m = saved[i];
        crow::json::wvalue row;
        row["name"] = ovl_title[i].value_or(m.name);
        row["slug"] = m.slug;
        row["plugin"] = m.plugin;
        row["cover"] = ovl[i].value_or(m.cover);
        // grid cover src is built SERVER-side (/cache/img?url={{cover_q}}), so the url
        // must arrive fully percent-encoded: its own `?token=..&ttl=..` would otherwise
        // be cut at the first `&` by the route's raw_url parse and the truncated fetch
        // returns an HTML error page that gets stored as the "cover". The detail page
        // has always encoded client-side (encodeURIComponent) — this is the same shape.
        row["cover_q"] = plugin::url_encode(ovl[i].value_or(m.cover));
        row["url"] = std::string("/view/manga/") + m.plugin + "/" + m.slug;
        // library-grid-stats 3.1: stat fields for grid badges/filters
        // (library-grid-controls 1.1 renders them; keys mirror /api/library).
        row["status"] = m.status;
        row["total_ch"] = m.total_ch;
        row["read_ch"] = m.read_ch;
        row["status_flags"] = m.status_flags; // bit0 carries the New state (was has_update)
        // display-dim 2.1: SSR the dim class -- the client paintDim repaint must
        // never be the first styler, or dimmed covers flash unstyled on refresh.
        row["dimmed"] = (m.status_flags & static_cast<int>(std::to_underlying(StatusFlag::kDim))) != 0;
        row["last_read_at"] = static_cast<double>(m.last_read_at);
        row["added_at"] = static_cast<double>(m.added_at);
        row["dup"] = m.dup_group; // library-dedup 3.1: grid duplicate filter/sort marker
        row["pinned"] = m.pinned; // library-pin: client splits the grid into Pinned/Library
        row["updated_at"] = static_cast<double>(m.updated_at);
        // library-search 1.1: client-side match corpus — display synopsis (D4
        // fallback, same rule as detail) + source alt titles verbatim JSON.
        row["syn"] = !m.synopsis_display.empty() ? m.synopsis_display : m.synopsis;
        row["alt"] = m.alt_titles;
        saved_rows.push_back(std::move(row));
    }
    ctx["saved_list"] = std::move(saved_rows);
    ctx["dup_total"] = std::to_string(dup_total); // library-dedup 4.1: sidebar duplicate count card
    ctx["saved_total"] = std::to_string(saved.size());
    ctx["saved_total_all"] = std::to_string(saved_total_all); // controls 2.1: sidebar total card

    spdlog::info("library plugin='{}' q='{}' page {} -> {} saved", plugin, q, page_no, saved.size());
    return ctx;
}

crow::response html(const std::string& file, const crow::json::wvalue& ctx) {
    crow::response r{crow::mustache::load(file).render(ctx)};
    // HTML shell references static-content snapshots the build can change; let the
    // browser revalidate instead of pinning a stale broken shell (see reader topbar).
    r.set_header("Cache-Control", "no-cache");
    return r;
}


} // namespace http
