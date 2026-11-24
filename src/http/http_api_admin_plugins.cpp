#include "api_ctx.hpp"

#include "img.hpp"

#include <SimpleIni.h>

#include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <system_error>
#include <vector>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace http::api {

void register_admin_plugins(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& covers = c.covers;

    // Dev endpoint: rescan the plugin dir, re-register, no restart.

    CROW_ROUTE(app, "/plugins/reload").methods(crow::HTTPMethod::POST)([&] {
        const int n = on_pool(db, [&] { return plugins.load_all(store); });
        crow::json::wvalue out;
        out["count"] = n;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Per-plugin cache clean (stale-shape recovery / refresh): drop cached
    // envelopes so the next call refetches. Body {"plugin": "id"}; absent = all.
    // Images are a separate store (disk LRU) and keep their own lifecycle.
    CROW_ROUTE(app, "/plugins/cache-clear").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
        std::string id;
        if (const auto body = crow::json::load(req.body); body && body.has("plugin")) {
            id = body["plugin"].s();
        }
        const int n = on_pool(db, [&] { return store.api_cache_clear(id); });
        crow::json::wvalue out;
        out["ok"] = n >= 0;
        out["cleared"] = n;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Page data for /plugins: registry rows + live health.
    CROW_ROUTE(app, "/api/plugins")
    ([&] {
        const auto rows = on_pool(db, [&] { return store.plugins(); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& p : rows) {
            crow::json::wvalue row;
            row["id"] = p.id;
            row["key"] = p.plugin_id;
            row["name"] = p.name;
            row["version"] = p.version;
            row["runtime"] = p.runtime;
            row["enabled"] = p.enabled;
            row["saved"] = p.saved; // library size for this plugin
            row["caps"] = p.caps;
            row["website"] = plugins.website(p.plugin_id);
            if (!p.enabled) {
                row["state"] = "disabled";
            } else {
                const plugin::CallHealth h = plugins.health(p.plugin_id);
                row["state"] = h.ok ? "active" : "error";
                row["error"] = h.error;
                row["checked_at"] = h.at;
            }
            list[idx++] = std::move(row);
        }
        // Enrichment sources: same shape as registry rows (minus Store id), so
        // the /plugins page renders them with identical markup; never toggleable.
        crow::json::wvalue enr_list(crow::json::type::List);
        int eidx = 0;
        for (const auto& e : plugins.enrichment_rows()) {
            crow::json::wvalue row;
            row["id"] = eidx; // synthetic, stable within this payload
            row["key"] = e.id;
            row["name"] = e.name;
            row["version"] = e.version;
            row["runtime"] = e.runtime;
            row["enabled"] = true;
            row["enrichment"] = true;
            row["caps"] = e.caps;
            row["website"] = e.website;
            row["state"] = e.ok ? "active" : "error";
            row["error"] = e.error;
            row["checked_at"] = e.checked_at;
            enr_list[eidx++] = std::move(row);
        }
        crow::json::wvalue out;
        out["plugins"] = std::move(list);
        out["enrichment"] = std::move(enr_list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Toggle registry flag; page + gates react on next request.
    CROW_ROUTE(app, "/plugin/<int>/enabled").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int id) {
        bool on = false;
        if (const auto body = crow::json::load(req.body); body && body.has("enabled")) {
            on = body["enabled"].b();
        }
        on_pool(db, [&] {
            store.set_plugin_enabled(id, on);
            return true; // on_pool is value-based; void lambdas need a token
        });
        crow::json::wvalue out;
        out["id"] = id;
        out["enabled"] = on;
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
            r.compressed = false; // logo bytes (png/jpeg/webp) already compressed
            r.set_header("Content-Type", image_mime(path));
            return r;
        }
        crow::json::wvalue p;
        for (const auto& k : req.url_params.keys()) {
            p[k] = req.url_params.get(k);
        }
        // Disabled gate on the db thread: no plugin-pool dispatch, error shape.
        const bool enabled = on_pool(db, [&] { return store.plugin_enabled(id); });
        if (!enabled) {
            crow::response r{R"({"ok":false,"error":"plugin disabled"})"};
            r.set_header("Content-Type", "application/json");
            return r;
        }
        const std::string params = p.dump();
        // Stale-serve wrap (design D3): the one call site, opt-in methods only.
        crow::response r{on_pool(plugin_pool, [&] {
            return call_cached(store, id, action, params, [&] { return plugins.call(id, action, params); }, &plugins);
        })};
        // Search covers also land in the cover pool (cache-first): the browser
        // grid asks /cache/img?cover=1, and the worker stores in the same v3
        // layout + [cache] image_format. ponytail: fire-and-forget, no wait.
        if (action == "search") {
            // search data shape: {items: [...], total: n} -- not a bare array.
            glz::generic parsed;
            if (glz::read_json(parsed, r.body) == glz::error_code{} && parsed.is_object()) {
                const auto& root = parsed.get_object();
                const auto* const d = root.find("data");
                if (d != root.end() && d->second.is_object()) {
                    const auto* const it = d->second.get_object().find("items");
                    if (it != d->second.get_object().end() && it->second.is_array()) {
                        std::vector<reader::CoverJob> jobs;
                        for (const auto& el : it->second.get_array()) {
                            if (!el.is_object()) {
                                continue;
                            }
                            const auto& obj = el.get_object();
                            const auto* const c2 = obj.find("cover");
                            if (c2 == obj.end()) {
                                continue;
                            }
                            const auto* s = std::get_if<std::string>(&c2->second.data);
                            if (s && !s->empty()) {
                                jobs.push_back({.url = *s, .plugin = id});
                            }
                        }
                        if (!jobs.empty()) {
                            covers.nudge(std::move(jobs));
                        }
                    }
                }
            }
        }
        r.set_header("Content-Type", "application/json");
        return r;
    });

}

} // namespace http::api
