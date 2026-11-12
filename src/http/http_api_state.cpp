#include "api_ctx.hpp"

#include <optional>

#include <string>
#include <vector>

namespace http::api {

// Per-chapter host flags (skip / read progress / batched state) -- the
// reader-window rerun on skip keeps the response honest (bodies verbatim
// from the http_api monolith).
void register_state(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& prefetch = c.prefetch;

    // Per-manga dim+blur cover. Key = host/dim/<plugin>/<slug> in the host
    // bucket; toggled from the detail page, applied to every cover img that
    // carries plugin+slug (grid/detail); search results have no slug and are
    // never dimmed.
    const auto dim_key = [](const std::string& plugin, const std::string& slug) {
        return std::string(kDimPrefix) + plugin + '/' + slug;
    };
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
                store.state_set(kStateHost, dim_key(plugin, slug), on ? "1" : "0");
                return 0;
            });
            crow::response r{on ? R"({"ok":true,"on":true})" : R"({"ok":true,"on":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });
    // GET list of dimmed manga: [["plugin","slug"], ...] for client-side paint.
    CROW_ROUTE(app, "/api/display/dim-list")([&] {
        const auto keys = on_pool(db, [&] { return store.state_keys_value(kStateHost, kDimPrefix, "1"); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& k : keys) {
            const auto rest = k.substr(std::string(kDimPrefix).size());
            const auto slash = rest.find('/');
            if (slash == std::string::npos || slash == 0) {
                continue;
            }
            crow::json::wvalue pair(crow::json::type::List);
            pair[0] = rest.substr(0, slash);
            pair[1] = rest.substr(slash + 1);
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
    CROW_ROUTE(app, "/chapter/<string>/skip")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req, std::string id) -> crow::response {
            bool skipped = false;
            if (const auto body = crow::json::load(req.body); body && body.has("skipped")) {
                const auto t = body["skipped"].t();
                if (t == crow::json::type::True || t == crow::json::type::False) {
                    skipped = body["skipped"].b();
                }
            }

            on_pool(db, [&] {
                store.state_set(kStateHost, std::string(kSkipPrefix) + id, skipped ? "1" : "0");
                return 0;
            });

            std::vector<crow::json::wvalue> window;
            const std::optional<ReaderPosition> last = on_pool(db, [&] { return store.position_last(); });
            if (last) {
                const std::string& plugin = last->plugin;
                const std::string& manga = last->slug;
                const std::string& chapter = last->chapter_id;
                if (!plugin.empty() && !manga.empty() && !chapter.empty()) {
                    const reader::Position where{.chapter_id = chapter, .page = last->page};
                    const std::vector<std::string> found =
                        reader_window(store, db, plugin_pool, plugins, plugin, manga, where);
                    for (const auto& url : found) {
                        window.emplace_back(url);
                    }
                    if (!found.empty()) {
                        prefetch.nudge(found); // rerun delta (3.3)
                    }
                }
            }

            crow::json::wvalue out;
            out["id"] = id;
            out["skipped"] = skipped;
            out["window"] = std::move(window);
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // Chapter read progress (chapter-list-controls 1.1): max pages read per
    // chapter under host/read/<uuid>; page 0 clears the marker.
    CROW_ROUTE(app, "/chapter/<string>/read")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req, std::string id) -> crow::response {
            const auto body = crow::json::load(req.body);
            const int page = static_cast<int>(std::max(0L, num_field(body, "page", 0)));
            on_pool(db, [&] {
                store.state_set(kStateHost, std::string(kReadPrefix) + id, std::to_string(page));
                return 0;
            });
            crow::json::wvalue out;
            out["id"] = id;
            out["read"] = page;
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // Batch read/skip state (chapter-list-controls 1.2): one round trip for
    // the detail page list. Absent keys omit the field.
    CROW_ROUTE(app, "/chapter/state").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        const auto body = crow::json::load(req.body);
        std::vector<std::string> ids;
        if (body && body.has("ids") && body["ids"].t() == crow::json::type::List) {
            for (const auto& v : body["ids"]) {
                if (v.t() == crow::json::type::String) {
                    ids.push_back(v.s());
                }
            }
        }
        crow::json::wvalue out;
        on_pool(db, [&] {
            for (const auto& id : ids) {
                if (const auto s = store.state_get(kStateHost, std::string(kSkipPrefix) + id); s && *s == "1") {
                    out[id]["skip"] = true;
                }
                if (const auto r = store.state_get(kStateHost, std::string(kReadPrefix) + id);
                    r && !r->empty() && *r != "0") {
                    out[id]["read"] = std::atoi(r->c_str());
                }
            }
            return 0;
        });
        crow::response res{out};
        res.set_header("Content-Type", "application/json");
        return res;
    });
}

} // namespace http::api
