#include "http.hpp"

#include <crow.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kPageSize = 12;

std::string param(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return value ? value : "";
}

int page_of(const std::string& text) {
    const int page = text.empty() ? 1 : std::atoi(text.c_str());
    return page < 1 ? 1 : page;
}

// Every database call runs on the single db worker, which owns the one connection.
template <class Fn>
auto on_db(BS::thread_pool<>& db, Fn fn) -> decltype(fn()) {
    auto done = std::make_shared<std::promise<decltype(fn())>>();
    db.detach_task([done, fn = std::move(fn)] {
        try {
            done->set_value(fn());
        } catch (...) {
            done->set_exception(std::current_exception());  // release the waiting future either way
        }
    });
    try {
        return done->get_future().get();
    } catch (const std::exception& e) {
        spdlog::error("on_db failed: {}", e.what());
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

    const auto plugins = on_db(db, [&] { return store.plugins(); });
    const int total = on_db(db, [&] { return store.count(plugin, q); });
    const int pages = std::max(1, (total + kPageSize - 1) / kPageSize);
    const int shown = std::min(page_no, pages);
    const auto titles = on_db(db, [&] { return store.search(plugin, q, kPageSize, (shown - 1) * kPageSize); });

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

}  // namespace

void serve(Store& store, BS::thread_pool<>& db, LogBuffer& logs, int port) {
    crow::SimpleApp app;
    crow::mustache::set_base("templates");

    // Crow's built-in static handler already serves /static/<path> from ./static.

    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.html", home_context(store, db, req)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.html", home_context(store, db, req)); });

    CROW_ROUTE(app, "/search")([] { return html("blank.html", layout("search")); });
    CROW_ROUTE(app, "/history")([] { return html("blank.html", layout("history")); });
    CROW_ROUTE(app, "/plugins")([] { return html("blank.html", layout("plugins")); });
    CROW_ROUTE(app, "/about")([] { return html("blank.html", layout("about")); });

    CROW_ROUTE(app, "/logs")
    ([&] {
        auto ctx = layout("logs");
        std::vector<crow::json::wvalue> lines;
        for (const auto& line : logs.snapshot()) lines.emplace_back(line);
        ctx["lines"] = std::move(lines);
        return html("logs.html", std::move(ctx));
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/log")
        .onopen([&](crow::websocket::connection& conn) {
            for (const auto& line : logs.snapshot()) conn.send_text(line);
            spdlog::info("log socket opened");
        });

    app.port(port).concurrency(4).run();}
