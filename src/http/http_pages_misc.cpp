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

void register_pages_misc(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& logs,
                    reader::CoverWorker& covers) {
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
