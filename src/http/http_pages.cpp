#include "http_routes.hpp"
#include "enrich_internal.hpp"

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace http {
namespace {

constexpr int kPageSize = 12;

crow::json::wvalue layout(Store& store, BS::thread_pool<>& db, const char* active) {
    crow::json::wvalue ctx;
    ctx["active_" + std::string(active)] = true;
    const auto dim = on_pool(db, [&] { return store.state_get(http::api::kStateHost, http::api::kDimBlurKey); });
    if (dim && *dim == "1") ctx["dim_blur"] = true;
    return ctx;
}


crow::json::wvalue home_context(Store& store, BS::thread_pool<>& db, const crow::request& req, reader::CoverWorker& covers) {
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

    std::vector<crow::json::wvalue> plugin_rows;
    for (const auto& item : plugins) {
        if (!item.enabled) {
            continue; // disabled: no sidebar entry, not searchable (user ruling)
        }
        crow::json::wvalue row;
        row["name"] = item.name;
        row["id"] = item.plugin_id; // client grid filter + ?plugin= deep-link key off plugin_id
        row["titles"] = std::to_string(saved_by_plugin[item.plugin_id]); // saved_manga is the Library; title search-cache is empty for plugin results
        row["current"] = item.plugin_id == plugin;
        if (!item.plugin_id.empty()) {
            row["logo"] = item.plugin_id; // absent key renders no <img> in the partial
        }
        plugin_rows.push_back(std::move(row));
    }
    ctx["plugins"] = std::move(plugin_rows);

    // library-save 4.1: saved entries are the Library — when any are saved,
    // the grid renders them (added_at desc) instead of the search-cache titles.
    const int saved_total_all = static_cast<int>(saved.size());
    // Cover prefetch: hand the saved covers to the cover worker; cache-first,
    // so entries already on disk are not re-fetched.
    // overlay chosen enrichment covers so the grid matches the detail page
    std::vector<std::optional<std::string>> ovl;
    ovl.reserve(saved.size());
    std::vector<std::optional<std::string>> ovl_title;
    for (const auto& m : saved) {
        ovl.push_back(http::api::chosen_cover(store, db, m.plugin, m.slug));
        ovl_title.push_back(http::api::chosen_title(store, db, m.plugin, m.slug));
    }
    std::vector<reader::CoverJob> jobs;
    for (std::size_t i = 0; i < saved.size(); ++i) {
        const std::string& c = ovl[i].has_value() ? *ovl[i] : saved[i].cover;
        if (!c.empty()) jobs.push_back({c, saved[i].plugin});
    }
    if (!jobs.empty()) covers.nudge(std::move(jobs));
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
        row["url"] = std::string("/view/manga/") + m.plugin + "/" + m.slug;
        // library-grid-stats 3.1: stat fields for grid badges/filters
        // (library-grid-controls 1.1 renders them; keys mirror /api/library).
        row["status"] = m.status;
        row["total_ch"] = m.total_ch;
        row["read_ch"] = m.read_ch;
        row["has_update"] = m.has_update;
        row["last_read_at"] = static_cast<double>(m.last_read_at);
        row["added_at"] = static_cast<double>(m.added_at);
        row["dup"] = m.dup_group; // library-dedup 3.1: grid duplicate filter/sort marker
        row["updated_at"] = static_cast<double>(m.updated_at);
        saved_rows.push_back(std::move(row));
    }
    ctx["saved_list"] = std::move(saved_rows);
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
    return {crow::mustache::load(file).render(ctx)};
}

} // namespace

void register_pages(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& logs, reader::CoverWorker& covers) {
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
        int ch = 1, page = 1;
        std::string mode, dir, zoom;
        if (pos) {
            ch = std::max(0, static_cast<int>(pos->chapter_number)); // ch 0 is valid
            page = std::max(1, pos->page);
            mode = std::move(pos->mode);
            dir = std::move(pos->direction);
            zoom = std::move(pos->zoom);
        }
        auto ctx = layout(store, db, "library");
        ctx["resume_ch"] = ch;
        ctx["resume_page"] = page;
        ctx["resume_mode"] = std::move(mode);
        ctx["resume_dir"] = std::move(dir);
        ctx["resume_zoom"] = std::move(zoom);
        return html("reader.mustache", std::move(ctx));
    });

    // manga-detail: the shelf page (metadata + chapter list, hydrating from
    // /api/*). Reader is at /read.
    CROW_ROUTE(app, "/view/manga/<string>/<string>")
    ([&](const crow::request&, std::string, std::string) {
        return html("detail.mustache", layout(store, db, "library"));
    });

    CROW_ROUTE(app, "/plugins")([&] { return html("plugins.mustache", layout(store, db, "plugins")); });
    CROW_ROUTE(app, "/settings")([&] { return html("settings.mustache", layout(store, db, "settings")); });
    CROW_ROUTE(app, "/about")([&] { return html("blank.mustache", layout(store, db, "about")); });

    CROW_ROUTE(app, "/logs")
    ([&] {
        auto ctx = layout(store, db, "logs");
        std::vector<crow::json::wvalue> lines;
        for (const auto& line : logs.snapshot()) {
            lines.emplace_back(line);
        }
        ctx["lines"] = std::move(lines);
        return html("logs.mustache", std::move(ctx));
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
crow::response br_asset(const crow::request& req, const char* path, const char* mime) {
    // Serve precompressed brotli body when the client advertises it; else
    // fall back to the plain sibling (must exist). Content-Encoding: br is
    // only set on the .br branch, never on the plain one.
    const std::string ae = req.get_header_value("Accept-Encoding");
    const bool wants_br = ae.find("br") != std::string::npos;
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
            crow::response r{404};
            return r;
        }
    }
    std::stringstream ss;
    ss << f.rdbuf();
    crow::response r{ss.str()};
    r.compressed = false; // precompressed (br or plain bytes) — never re-deflate
    r.set_header("Content-Type", mime);
    if (served_br) r.set_header("Content-Encoding", "br");
    r.set_header("Cache-Control", "public, max-age=31536000, immutable");
    return r;
}
} // namespace

void http::register_static(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/static/htmx.min.js")([](const crow::request& req) {
        return br_asset(req, "static/htmx.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/alpine.min.js")([](const crow::request& req) {
        return br_asset(req, "static/alpine.min.js", "application/javascript");
    });
    CROW_ROUTE(app, "/static/tailwind.min.js")([](const crow::request& req) {
        return br_asset(req, "static/tailwind.min.js", "application/javascript");
    });
}
