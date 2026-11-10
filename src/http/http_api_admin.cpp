#include "api_ctx.hpp"

#include "img.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace http {
namespace api {

// Plugin registry/admin + config endpoints (split from the http_api
// monolith; route bodies verbatim).
void register_admin(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& covers = c.covers;
    auto& ini_path = c.ini_path;
    auto& overrides_ref = c.overrides;

    // Dev endpoint: rescan the plugin dir, re-register, no restart.
    CROW_ROUTE(app, "/plugins/reload").methods(crow::HTTPMethod::POST)([&] {
        const int n = on_pool(db, [&] { return plugins.load_all(store); });
        crow::json::wvalue out;
        out["count"] = n;
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
            row["titles"] = p.titles;
            row["caps"] = p.caps;
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
        crow::json::wvalue out;
        out["plugins"] = std::move(list);
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
            return call_cached(store, id, action, params, [&] { return plugins.call(id, action, params); });
        })};
        // Search covers also land in the cover pool (cache-first): the browser
        // grid asks /cache/img?cover=1, and the worker stores in the same v3
        // layout + [cache] image_format. ponytail: fire-and-forget, no wait.
        if (action == "search") {
            // search data shape: {items: [...], total: n} -- not a bare array.
            glz::generic parsed;
            if (glz::read_json(parsed, r.body) == glz::error_code{} && parsed.is_object()) {
                const auto& root = parsed.get_object();
                const auto d = root.find("data");
                if (d != root.end() && d->second.is_object()) {
                    const auto it = d->second.get_object().find("items");
                    if (it != d->second.get_object().end() && it->second.is_array()) {
                        std::vector<reader::CoverJob> jobs;
                        for (const auto& el : it->second.get_array()) {
                            if (!el.is_object()) continue;
                            const auto& obj = el.get_object();
                            const auto c2 = obj.find("cover");
                            if (c2 == obj.end()) continue;
                            const auto* s = std::get_if<std::string>(&c2->second.data);
                            if (s && !s->empty()) jobs.push_back({*s, id});
                        }
                        if (!jobs.empty()) covers.nudge(std::move(jobs));
                    }
                }
            }
        }
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Config read (settings page): registry projection with layer provenance.
    // Re-resolves per request (ini may change on disk); no writes.
    CROW_ROUTE(app, "/config").methods(crow::HTTPMethod::GET)([&] {
        crow::json::wvalue knobs(crow::json::type::List);
        int i = 0;
        for (const auto& d : config::describe(ini_path, overrides_ref)) {
            crow::json::wvalue k;
            k["section"] = d.section;
            k["key"] = d.key;
            k["fullKey"] = d.full_key;
            k["doc"] = d.doc;
            k["type"] = d.type == config::Type::Str ? "str" : (d.type == config::Type::Int ? "int" : "int64");
            k["flag"] = d.flag;
            k["env"] = d.env;
            k["default"] = d.def;
            k["value"] = d.value;
            k["source"] = d.source;
            knobs[i++] = std::move(k);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["knobs"] = std::move(knobs);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Config save (D5): validation-first merge, comments/ordering survive.
    // Body {"section.key": string|number} in resolved full-key form.
    CROW_ROUTE(app, "/config").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        auto fail = [](int code, std::string msg) {
            crow::json::wvalue e;
            e["ok"] = false;
            e["error"] = std::move(msg);
            crow::response r{std::move(e)};
            r.code = code;
            r.set_header("Content-Type", "application/json");
            return r;
        };

        std::map<std::string, std::string> updates;
        std::string parse_err;
        if (!config::parse_updates(req.body, updates, parse_err))
            return fail(400, std::move(parse_err));

        std::string err;
        const int written = config::save(updates, ini_path, &err);
        if (written < 0)
            return fail(written == -2 ? 500 : 400, std::move(err));

        // Live default (task 1.2): the running Host picks up the new
        // plugin.fetch_profile without a restart.
        if (const auto it = updates.find("plugin.fetch_profile"); it != updates.end()) {
            plugins.set_default_profile(it->second);
        }

        crow::json::wvalue out;
        out["ok"] = true;
        out["written"] = written;
        crow::response r{std::move(out)};
        r.set_header("Content-Type", "application/json");
        return r;
    });
}

} // namespace api
} // namespace http
