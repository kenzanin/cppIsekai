#include "enrich_internal.hpp"
#include "http_routes.hpp"

#include "plugin/url_codec.hpp"

#include <glaze/glaze.hpp>
#include <optional>
#include <utility>

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace http {
namespace {

constexpr int kPageSize = 12;

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
    const int total = on_pool(db, [&] { return store.count(plugin, q); });
    const int pages = std::max(1, (total + kPageSize - 1) / kPageSize);
    const int shown = std::min(page_no, pages);
    const auto titles = on_pool(db, [&] { return store.search(plugin, q, kPageSize, (shown - 1) * kPageSize); });

    auto ctx = layout(store, db, "library");
    ctx["q"] = q;
    ctx["plugin"] = plugin;
    ctx["total"] = std::to_string(total);
    ctx["page"] = std::to_string(shown);
    ctx["pages"] = std::to_string(pages);
    ctx["prev"] = std::to_string(shown > 1 ? shown - 1 : 1);
    ctx["next"] = std::to_string(shown < pages ? shown + 1 : pages);
    ctx["has_prev"] = shown > 1;
    ctx["has_next"] = shown < pages;
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

    std::vector<crow::json::wvalue> cards;
    for (const auto& title : titles) {
        crow::json::wvalue card;
        card["name"] = title.name;
        card["plugin"] = title.plugin;
        card["hue"] = std::to_string(static_cast<int>(title.id * 47 % 360));
        cards.push_back(std::move(card));
    }
    ctx["titles"] = std::move(cards);
    spdlog::info("library plugin='{}' q='{}' page {} -> {} titles", plugin, q, shown, titles.size());
    return ctx;
}

crow::response html(const std::string& file, const crow::json::wvalue& ctx) {
    crow::response r{crow::mustache::load(file).render(ctx)};
    // HTML shell references static-content snapshots the build can change; let the
    // browser revalidate instead of pinning a stale broken shell (see reader topbar).
    r.set_header("Cache-Control", "no-cache");
    return r;
}

} // namespace

void register_pages(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& logs,
                    reader::CoverWorker& covers) {
    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.mustache", home_context(store, db, req, covers)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.mustache", home_context(store, db, req, covers)); });

    CROW_ROUTE(app, "/search")([&] { return html("search.mustache", layout(store, db, "search")); });

    // reader (reader-page 3.2): the reader page at /read. Server-side shell
    // only (D1): pages load from /api/pages. The store seed is resume-only
    // (db worker, ponytail in store.hpp); ?ch/?page in the URL override it
    // client-side in boot(), and empty mode/direction leave the JS defaults
    // (paged rtl) untouched.
    CROW_ROUTE(app, "/read/<string>/<string>")
    ([&](const crow::request&, const std::string& plugin, const std::string& slug) {
        const auto pos = on_pool(db, [&] { return store.position_get(plugin, slug); });
        int ch = 1;
        int page = 1;
        std::string mode;
        std::string dir;
        std::string zoom;
        if (pos) {
            ch = std::max(0, static_cast<int>(pos->chapter_number)); // ch 0 is valid
            page = std::max(1, pos->page);
            mode = pos->mode;
            dir = pos->direction;
            zoom = pos->zoom;
        }
        auto ctx = layout(store, db, "library");
        ctx["resume_ch"] = ch;
        ctx["resume_page"] = page;
        ctx["resume_mode"] = mode;
        ctx["resume_dir"] = dir;
        ctx["resume_zoom"] = zoom;
        return html("reader.mustache", ctx);
    });

    // manga-detail: the shelf page (metadata + chapter list, hydrating from
    // /api/*). Reader is at /read.
    CROW_ROUTE(app, "/view/manga/<string>/<string>")
    ([&](const crow::request&, std::string plugin, const std::string&) {
        auto ctx = layout(store, db, "library");
        // 0 = disabled: the page's manual-update button renders dead (plugin-lifecycle 1.3)
        ctx["pe"] = on_pool(db, [&] { return store.plugin_enabled(plugin); }) ? 1 : 0;
        return html("detail.mustache", ctx);
    });

    // library-migrate 1.1: source picker shell; results hydrate client-side
    // exactly like the search page (per-plugin /plugin/<id>/search), then
    // POST /api/migrate rekeys the row server-side.
    CROW_ROUTE(app, "/view/migrate/<string>/<string>")
    ([&](const crow::request&, std::string plugin, std::string slug) -> crow::response {
        const auto row = on_pool(db, [&] { return store.saved_get(plugin, slug); });
        if (!row) { // unsaved/deleted pair: back to the detail page it came from
            crow::response r;
            r.code = 302;
            r.set_header("Location", "/view/manga/" + plugin + "/" + slug);
            return r;
        }
        auto ctx = layout(store, db, "library");
        ctx["plugin"] = plugin;
        ctx["slug"] = slug;
        ctx["name"] = row->name;
        ctx["src_ch"] = row->total_ch;
        ctx["src_read"] = row->read_ch;
        // exact-badge corpus: normalized name + alt titles, comma-joined (the
        // keys are [a-z0-9]+ so the separator cannot collide)
        std::string keys = title_key(row->name);
        glz::generic arr;
        if (!row->alt_titles.empty() && glz::read_json(arr, row->alt_titles) == glz::error_code{} && arr.is_array()) {
            for (const auto& el : arr.get_array()) {
                if (const auto* s = std::get_if<std::string>(&el.data); s != nullptr && !s->empty()) {
                    const std::string k = title_key(*s);
                    if (!k.empty()) {
                        keys += ',';
                        keys += k;
                    }
                }
            }
        }
        ctx["altkeys"] = keys;
        return html("migrate.mustache", ctx);
    });

    CROW_ROUTE(app, "/plugins")([&] { return html("plugins.mustache", layout(store, db, "plugins")); });
    CROW_ROUTE(app, "/settings")([&] { return html("settings.mustache", layout(store, db, "settings")); });
   CROW_ROUTE(app, "/about")([&] { return html("about.mustache", layout(store, db, "about")); });
   // Single source for the About page: repo-root README.md, read per
   // request (it changes with every push) and cached never.
   CROW_ROUTE(app, "/about/readme")([](const crow::request& /*req*/, crow::response& res) {
       std::ifstream f("README.md", std::ios::binary);
       if (!f) { res.code = 404; res.end(); return; }
       std::stringstream ss;
       ss << f.rdbuf();
       res.body = ss.str();
       res.compressed = false;
       res.set_header("Content-Type", "text/markdown; charset=utf-8");
       res.set_header("Cache-Control", "no-cache");
       res.end();
   });

    CROW_ROUTE(app, "/logs")
    ([&] {
        auto ctx = layout(store, db, "logs");
        std::vector<crow::json::wvalue> lines;
        for (const auto& line : logs.snapshot()) {
            lines.emplace_back(line);
        }
        ctx["lines"] = std::move(lines);
        return html("logs.mustache", ctx);
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/log").onopen([&](crow::websocket::connection& conn) {
        for (const auto& line : logs.snapshot()) {
            conn.send_text(line);
        }
        spdlog::info("log socket opened");
    });
}

} // namespace http

namespace {
void br_asset(const crow::request& req, crow::response& res, const char* path, const char* mime,
              const char* cache = "public, max-age=31536000, immutable") {
    // Serve precompressed brotli body when the client advertises it; else
    // fall back to the plain sibling (must exist). Content-Encoding: br is
    // only set on the .br branch, never on the plain one.
    // Out-param on purpose: crow's response move assignment drops the
    // `compressed` flag, so a response returned by value would get gzip'd on
    // top of the brotli body (browser decodes gzip, gets brotli, JS explodes).
    const std::string ae = req.get_header_value("Accept-Encoding");
    const bool wants_br = ae.contains("br");
    // Vendored assets ship .br only; fall back to it even for clients that
    // did not advertise br (brotli is universal in modern browsers).
    std::string file = wants_br ? std::string(path) + ".br" : std::string(path);
    std::ifstream f(file, std::ios::binary);
    bool served_br = wants_br;
    if (!f) {
        file = std::string(path) + ".br";
        f.open(file, std::ios::binary);
        served_br = true;
        if (!f) {
            res.code = 404;
            res.end();
            return;
        }
    }
    std::stringstream ss;
    ss << f.rdbuf();
    res.body = ss.str();
    res.compressed = false; // precompressed (br or plain bytes) — never re-deflate
    res.set_header("Content-Type", mime);
    if (served_br) {
        res.set_header("Content-Encoding", "br");
    }
    res.set_header("Cache-Control", cache);
    res.end();
}
} // namespace

void http::register_static(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/static/htmx.min.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/htmx.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/marked.min.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/marked.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/mermaid.min.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/mermaid.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/alpine.min.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/alpine.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/tailwind.min.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/tailwind.min.js", "application/javascript");
    });
    // App JS: revalidate each load -- these change with releases, unlike vendored libs.
    CROW_ROUTE(app, "/static/detail.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/detail.js", "application/javascript", "no-cache");
    });
    CROW_ROUTE(app, "/static/reader.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/reader.js", "application/javascript", "no-cache");
    });
}
