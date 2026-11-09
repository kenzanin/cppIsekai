#include "http_routes.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <vector>

namespace http {
namespace {

constexpr int kPageSize = 12;

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
        if (!item.enabled) {
            continue; // disabled: no sidebar entry, not searchable (user ruling)
        }
        crow::json::wvalue row;
        row["name"] = item.name;
        row["titles"] = std::to_string(item.titles);
        row["current"] = item.name == plugin;
        if (!item.plugin_id.empty()) {
            row["logo"] = item.plugin_id; // absent key renders no <img> in the partial
        }
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

crow::response html(const std::string& file, const crow::json::wvalue& ctx) {
    return {crow::mustache::load(file).render(ctx)};
}

} // namespace

void register_pages(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& logs) {
    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.mustache", home_context(store, db, req)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.mustache", home_context(store, db, req)); });

    CROW_ROUTE(app, "/search")([&] { return html("search.mustache", layout("search")); });

    // manga-detail 2.2: the detail page. Server-side shell only (D1): the
    // metadata/chapters hydrate from /api/*; slug segments pass through for
    // Alpine to rebuild the API URLs (the endpoints re-guard them).
    CROW_ROUTE(app, "/view/manga/<string>/<string>")
    ([](const crow::request&, std::string, std::string) { return html("detail.mustache", layout("library")); });

    CROW_ROUTE(app, "/history")([] { return html("blank.mustache", layout("history")); });
    CROW_ROUTE(app, "/plugins")([&] { return html("plugins.mustache", layout("plugins")); });
    CROW_ROUTE(app, "/about")([] { return html("blank.mustache", layout("about")); });

    CROW_ROUTE(app, "/logs")
    ([&] {
        auto ctx = layout("logs");
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
