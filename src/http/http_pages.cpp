#include "enrich_internal.hpp"
#include "http_routes.hpp"

#include "assets/assets.hpp"
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

#include "http_pages_context.hpp"

namespace http {


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

    // api-first: the log ring is also exposed as JSON so every feature the
    // /logs page shows is readable with curl (the page itself is HTML; the
    // websocket is the live tail).
    CROW_ROUTE(app, "/api/logs")
    ([&] {
        crow::json::wvalue lines(crow::json::type::List);
        int i = 0;
        for (const auto& line : logs.snapshot()) lines[i++] = line;
        crow::json::wvalue out;
        out["ok"] = true;
        out["lines"] = std::move(lines);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
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
    std::string body;
    bool served_br = false;
    bool found = false;
    // [assets] in-memory root wins when present; .br first, plain next.
    if (const auto* mem = assets::read(std::string(path) + ".br")) {
        body = *mem;
        served_br = true;
        found = true;
    } else if (const auto* mem = assets::read(path)) {
        body = *mem;
        found = true;
    }
    if (!found) {
        // Vendored assets ship .br only; fall back to it even for clients that
        // did not advertise br (brotli is universal in modern browsers).
        std::string file = wants_br ? std::string(path) + ".br" : std::string(path);
        std::ifstream f(file, std::ios::binary);
        if (!f) {
            file = std::string(path) + ".br";
            f.open(file, std::ios::binary);
            served_br = true;
        } else {
            served_br = wants_br;
        }
        if (!f) {
            res.code = 404;
            res.end();
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        body = ss.str();
    }
    res.body = std::move(body);
    res.compressed = false; // precompressed (br or plain bytes) — never re-deflate
    res.set_header("Content-Type", mime);
    if (served_br) {
        res.set_header("Content-Encoding", "br");
    }
    res.set_header("Cache-Control", cache);
    res.end();
}

crow::response favicon_response() {
    crow::response res;
    const std::string* mem = assets::read("icon/tray.png");
    if (mem == nullptr) {
        mem = assets::read("assets/tray.png"); // pack-assets keeps the repo path
    }
    std::string body;
    if (mem != nullptr) {
        body = *mem;
    } else {
        std::ifstream f("assets/tray.png", std::ios::binary);
        if (!f) {
            res.code = 404;
            return res;
        }
        body.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    res.body = std::move(body);
    res.set_header("Content-Type", "image/png");
    res.set_header("Cache-Control", "public, max-age=86400");
    return res;
}
} // namespace

void http::register_static(crow::SimpleApp& app) {
    // The browser tab icon is the same PNG the system tray uses, so the two
    // identities never drift. Both names are served for old bookmarks.
    CROW_ROUTE(app, "/favicon.png")([] { return favicon_response(); });
    CROW_ROUTE(app, "/favicon.ico")([] { return favicon_response(); });
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
    // Compiled Tailwind CSS (no CDN-style runtime JIT in the browser).
    // Revalidate each load: the stylesheet is regenerated by `xmake js`.
    CROW_ROUTE(app, "/static/app.css")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/app.css", "text/css", "no-cache");
    });
    // App JS: revalidate each load -- these change with releases, unlike vendored libs.
    CROW_ROUTE(app, "/static/detail.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/detail.js", "application/javascript", "no-cache");
    });
    CROW_ROUTE(app, "/static/reader.js")([](const crow::request& req, crow::response& res) {
        br_asset(req, res, "static/reader.js", "application/javascript", "no-cache");
    });
}
