#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "plugin/url_codec.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace http::api {

void register_state_display(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;


    // Per-manga dim+blur cover lives on the saved row itself: status_flags
    // bit6 (is_dim), toggled from the detail page, applied to every cover
    // img that carries plugin+slug (grid/detail). Unsaved manga have no row,
    // so the toggle is a no-op there (the request still succeeds).
    CROW_ROUTE(app, "/api/display/dim-blur")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            if (!body || !body.has("plugin") || !body.has("slug")) {
                return crow::response{400};
            }
            const std::string plugin = body["plugin"].s();
            const std::string slug = body["slug"].s();
            if (plugin.empty() || plugin.contains('/') || slug.empty()) {
                return crow::response{400};
            }
            bool on = false;
            if (body.has("on")) {
                on = body["on"].t() == crow::json::type::True;
            }
            on_pool(db, [&] {
                store.saved_dim_update(plugin, slug, on);
                return 0;
            });
            crow::response r{on ? R"({"ok":true,"on":true})" : R"({"ok":true,"on":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });
    // GET list of dimmed manga: [["plugin","slug"], ...] for client-side paint.
    CROW_ROUTE(app, "/api/display/dim-list")([&] {
        const auto pairs = on_pool(db, [&] { return store.saved_dim_list(); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& [plugin, slug] : pairs) {
            crow::json::wvalue pair(crow::json::type::List);
            pair[0] = plugin;
            pair[1] = slug;
            list[idx++] = std::move(pair);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["data"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Chapter skip (image-cache): the flag lands under reserved
    // host/skip/<uuid>; the window re-runs against the last position so the
    // response already reflects the reroute.
}

} // namespace http::api
