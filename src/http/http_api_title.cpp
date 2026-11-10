#include "api_ctx.hpp"

#include <ctime>

namespace http {
namespace api {
namespace {

// The shared body of the detail/chapters endpoints: enabled-gate, stale-serve,
// json envelope. `method` is the plugin method; `id_key` the query param the
// plugin's contract names for the title key.
crow::response title_data_response(plugin::Host& plugins, BS::thread_pool<>& db,
                                   BS::thread_pool<>& plugin_pool, Store& store, const std::string& plugin,
                                   const std::string& slug, const std::string& method) {
    if (!valid_title_pair(plugin, slug)) {
        return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
    }
    const bool enabled = on_pool(db, [&] { return store.plugin_enabled(plugin); });
    if (!enabled) {
        crow::response r{R"({"ok":false,"error":"plugin disabled"})"};
        r.set_header("Content-Type", "application/json");
        return r;
    }
    crow::json::wvalue p;
    p["id"] = slug; // the slug IS the title key the cache layout and plugins mint
    const std::string params = p.dump();
    crow::response r{on_pool(plugin_pool, [&] {
        return call_cached(store, plugin, method, params, [&] { return plugins.call(plugin, method, params); });
    })};
    r.set_header("Content-Type", "application/json");
    return r;
}

} // namespace

// manga-detail 1.1 detail data + related + chapters + forced refresh
// (split from the http_api monolith; bodies verbatim).
void register_title(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;

    // manga-detail 1.1: the detail-page data endpoints. Same envelope rules as
    // /plugin/<id>/<method>; keyed by our own slug, which is the title key the
    // cache layout and plugins mint. The page's Alpine layer consumes these.
    CROW_ROUTE(app, "/api/detail/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "detail");
    });

    // detail-enrichment 2.1: related readings; same envelope and stale-serve
    // rules as /api/detail, plugin method "related".
    CROW_ROUTE(app, "/api/related/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "related");
    });

    CROW_ROUTE(app, "/api/chapters/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "chapters");
    });

    // manga-detail 1.2: forced refresh (design D3). Call-through both methods,
    // overwrite the stale entries on success; the first failure returns it and
    // leaves every cached entry untouched.
    CROW_ROUTE(app, "/api/refresh/<string>/<string>").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) -> crow::response {
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        const bool enabled = on_pool(db, [&] { return store.plugin_enabled(plugin); });
        if (!enabled) {
            crow::response r{R"({"ok":false,"error":"plugin disabled"})"};
            r.set_header("Content-Type", "application/json");
            return r;
        }
        return crow::response{on_pool(plugin_pool, [&] {
            crow::json::wvalue p;
            p["id"] = slug;
            const std::string params = p.dump();
            std::string fresh[2];
            const char* methods[2] = {"detail", "chapters"};
            for (int i = 0; i < 2; ++i) {
                fresh[i] = plugins.call(plugin, methods[i], params);
                glz::generic env;
                if (glz::read_json(env, fresh[i]) || !envelope_ok(env)) {
                    const std::string body =
                        std::string("{\"ok\":false,\"error\":\"refresh failed: ") + methods[i] + "\"}";
                    crow::response r{502, body}; // upstream plugin call failed, not the route
                    r.set_header("Content-Type", "application/json");
                    return r; // cache untouched: nothing stored yet
                }
            }
            for (int i = 0; i < 2; ++i) {
                const std::string key = stale_key(plugin, methods[i], params);
                // Same key shape call_cached persists under; write-through here
                // keeps refresh and lazy cache consistent. db worker (store is not
                // thread-safe) — plugin_pool hands back to on_pool(db).
                const std::string body = fresh[i];
                on_pool(db, [&] {
                    store.api_cache_put(plugin, key, params, body,
                                        static_cast<std::int64_t>(std::time(nullptr)), kApiCacheTtl);
                    return 0;
                });
            }
            crow::response r{R"({"ok":true})"};
            r.set_header("Content-Type", "application/json");
            return r;
        })};
    });
}

} // namespace api
} // namespace http
