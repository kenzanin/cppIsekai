#include "http.hpp"

#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <crow.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int kPageSize = 12;

const char* logo_mime(std::string_view path) {
    const auto ext = path.substr(path.find_last_of('.') + 1);
    if (ext == "png") {
        return "image/png";
}
    if (ext == "svg") {
        return "image/svg+xml";
}
    if (ext == "jpg" || ext == "jpeg") {
        return "image/jpeg";
}
    if (ext == "avif") {
        return "image/avif";
}
    return "application/octet-stream";
}

std::string param(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return (value != nullptr) ? value : "";
}

int page_of(const std::string& text) {
    const int page = text.empty() ? 1 : std::atoi(text.c_str());
    return page < 1 ? 1 : page;
}

// Every database call runs on the single db worker, which owns the one connection.
// Same wrapper serves the plugin pool: one future hop, caller blocks.
template <class Fn> auto on_pool(BS::thread_pool<>& pool, Fn fn) -> decltype(fn()) {
    auto done = std::make_shared<std::promise<decltype(fn())>>();
    pool.detach_task([done, fn = std::move(fn)] {
        try {
            done->set_value(fn());
        } catch (...) {
            done->set_exception(std::current_exception()); // release the waiting future either way
        }
    });
    try {
        return done->get_future().get();
    } catch (const std::exception& e) {
        spdlog::error("on_pool failed: {}", e.what());
        throw;
    }
}

crow::json::wvalue layout(const char* active) {
    crow::json::wvalue ctx;
    ctx["active_" + std::string(active)] = true;
    return ctx;
}

crow::json::wvalue home_context(Store& store, BS::thread_pool<>& db, const crow::request& req) {
    const std::string plugin = param(req, "plugin");
    const std::string q = param(req, "q");
    const int page_no = page_of(param(req, "page"));

    const auto plugins = on_pool(db, [&] { return store.plugins(); });
    const int total = on_pool(db, [&] { return store.count(plugin, q); });
    const int pages = std::max(1, (total + kPageSize - 1) / kPageSize);
    const int shown = std::min(page_no, pages);
    const auto titles = on_pool(db, [&] { return store.search(plugin, q, kPageSize, (shown - 1) * kPageSize); });

    auto ctx = layout("library");
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

    std::vector<crow::json::wvalue> plugin_rows;
    for (const auto& item : plugins) {
        crow::json::wvalue row;
        row["name"] = item.name;
        row["titles"] = std::to_string(item.titles);
        row["current"] = item.name == plugin;
        if (!item.plugin_id.empty())
            row["logo"] = item.plugin_id; // absent key renders no <img> in the partial
        plugin_rows.push_back(std::move(row));
    }
    ctx["plugins"] = std::move(plugin_rows);

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

crow::response html(const std::string& file, crow::json::wvalue ctx) {
    return crow::response(crow::mustache::load(file).render(ctx));
}

} // namespace

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
           LogBuffer& logs, int port) {
    crow::SimpleApp app;
    crow::mustache::set_base("templates");

    // Crow's built-in static handler already serves /static/<path> from ./static.

    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.mustache", home_context(store, db, req)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.mustache", home_context(store, db, req)); });

    CROW_ROUTE(app, "/search")([] { return html("blank.mustache", layout("search")); });
    CROW_ROUTE(app, "/history")([] { return html("blank.mustache", layout("history")); });
    CROW_ROUTE(app, "/plugins")([] { return html("blank.mustache", layout("plugins")); });
    CROW_ROUTE(app, "/about")([] { return html("blank.mustache", layout("about")); });

    // Dev endpoint: rescan the plugin dir, re-register, no restart.
    CROW_ROUTE(app, "/plugins/reload").methods(crow::HTTPMethod::POST)([&] {
        const int n = on_pool(db, [&] { return plugins.load_all(store); });
        crow::json::wvalue out;
        out["count"] = n;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // /plugin/<id>/logo serves the logo file; any other <action> is an ABI
    // envelope call with the query string as the params object (debug path).
    CROW_ROUTE(app, "/plugin/<string>/<string>")
    ([&](const crow::request& req, std::string id, std::string action) -> crow::response {
        if (action == "logo") {
            const std::string path = plugins.logo_path(id);
            if (path.empty()) {
                crow::response r;
                r.code = 404;
                r.body = "no logo\n";
                return r;
            }
            std::ifstream f(path, std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            crow::response r{ss.str()};
            r.set_header("Content-Type", logo_mime(path));
            return r;
        }
        crow::json::wvalue p;
        for (const auto& k : req.url_params.keys())
            p[k] = req.url_params.get(k);
        crow::response r{on_pool(plugin_pool, [&] { return plugins.call(id, action, p.dump()); })};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/logs")
    ([&] {
        auto ctx = layout("logs");
        std::vector<crow::json::wvalue> lines;
        for (const auto& line : logs.snapshot())
            lines.emplace_back(line);
        ctx["lines"] = std::move(lines);
        return html("logs.mustache", std::move(ctx));
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/log").onopen([&](crow::websocket::connection& conn) {
        for (const auto& line : logs.snapshot())
            conn.send_text(line);
        spdlog::info("log socket opened");
    });

    app.port(port).concurrency(4).run();
}
