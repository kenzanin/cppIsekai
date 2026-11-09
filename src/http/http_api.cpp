#include "http_routes.hpp"

#include "config/config.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "stale.hpp"
#include "reader/window.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace http {
namespace {

// Reserved plugin_state rows for host bookkeeping: per-chapter skip flags and
// the last reader position (the anchor the skip-rerun reads back).
constexpr const char* kStateHost = "host";
constexpr const char* kSkipPrefix = "host/skip/";
constexpr const char* kPositionKey = "host/pos";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.

// manga-detail 1.1: the {plugin, slug} path pair guard. Slugs are our own
// cache-layout keys (fs-safe lowercase); anything carrying separators or dot
// segments is not a slug we minted -> 404 before any plugin or fs touch
// (design D5). Returns false when the pair needs the not-found response.
inline bool valid_title_pair(const std::string& plugin, const std::string& slug) {
    const auto ok = [](const std::string& s) {
        if (s.empty() || s.size() > 128 || s != s.substr(0, s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_.") )) {
            return false;
        }
        if (s.find("..") != std::string::npos) {
            return false;
        }
        return true;
    };
    return ok(plugin) && ok(slug);
}

// The shared body of the detail/chapters endpoints: enabled-gate, stale-serve,
// json envelope. `method` is the plugin method; `id_key` the query param the
// plugin's contract names for the title key.
crow::response title_data_response(plugin::Host& plugins, cache::Cache& images, BS::thread_pool<>& db,
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
        return call_cached(images, plugin, method, params, [&] { return plugins.call(plugin, method, params); });
    })};
    r.set_header("Content-Type", "application/json");
    return r;
}
std::string str_field(const crow::json::rvalue& j, const char* key) {
    return j.has(key) && j[key].t() == crow::json::type::String ? std::string(j[key].s()) : std::string{};
}
long num_field(const crow::json::rvalue& j, const char* key, long def) {
    return j.has(key) && j[key].t() == crow::json::type::Number ? j[key].i() : def;
}

// The reader window composition (image-cache): chapter list + pages through
// the cached ABI path (stale-serve included), skip flags from plugin_state,
// math in reader::next_window. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
std::vector<std::string> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                       plugin::Host& plugins, cache::Cache& images, const std::string& plugin_id,
                                       const std::string& manga_id, const reader::Position& pos) {
    crow::json::wvalue query;
    query["id"] = manga_id;
    const std::string chapters_query = query.dump();

    const std::string envelope = on_pool(plugin_pool, [&] {
        return call_cached(images, plugin_id, "chapters", chapters_query,
                           [&] { return plugins.call(plugin_id, "chapters", chapters_query); });
    });

    std::vector<reader::Chapter> chapters;
    if (const auto data = envelope_data(envelope)) {
        for (const auto& item : *data) {
            if (!item.is_object())
                continue;
            const auto& entry = item.get_object();
            const auto id = entry.find("id");
            const auto* id_s = id == entry.end() ? nullptr : std::get_if<std::string>(&id->second.data);
            if (!id_s)
                continue;
            double number = 0; // fractional chapter numbers are a thing
            if (const auto num = entry.find("number"); num != entry.end())
                if (const auto* d = std::get_if<double>(&num->second.data))
                    number = *d;
            chapters.push_back(reader::Chapter{*id_s, number});
        }
    }
    if (chapters.empty()) {
        spdlog::warn("reader: no chapters for plugin='{}' manga='{}'", plugin_id, manga_id);
        return {};
    }

    reader::Skipped skip;
    on_pool(db, [&] {
        for (const auto& ch : chapters)
            if (const std::optional<std::string> v = store.state_get(kStateHost, std::string(kSkipPrefix) + ch.id))
                skip[ch.id] = *v == "1";
        return 0; // on_pool is value-based
    });

    const auto pages_of = [&](const std::string& chapter_id) {
        crow::json::wvalue q;
        q["id"] = chapter_id;
        const std::string pages_query = q.dump();
        const std::string env = on_pool(plugin_pool, [&] {
            return call_cached(images, plugin_id, "pages", pages_query,
                               [&] { return plugins.call(plugin_id, "pages", pages_query); });
        });
        std::vector<std::string> urls;
        if (const auto data = envelope_data(env))
            for (const auto& el : *data)
                if (const auto* s = std::get_if<std::string>(&el.data))
                    urls.emplace_back(*s);
        return urls;
    };

    const std::vector<std::string> window = reader::next_window(chapters, pages_of, pos, skip);
    spdlog::info("reader window {} p{} -> {} page(s)", pos.chapter_id, pos.page, window.size());
    return window;
}

} // namespace

void register_api(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                  plugin::Host& plugins, cache::Cache& images, reader::Worker& prefetch, const std::string& ini_path) {
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
            return call_cached(images, id, action, params, [&] { return plugins.call(id, action, params); });
        })};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // manga-detail 1.1: the detail-page data endpoints. Same envelope rules as
    // /plugin/<id>/<method>; keyed by our own slug, which is the title key the
    // cache layout and plugins mint. The page's Alpine layer consumes these.
    CROW_ROUTE(app, "/api/detail/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, images, db, plugin_pool, store, plugin, slug, "detail");
    });

    CROW_ROUTE(app, "/api/chapters/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, images, db, plugin_pool, store, plugin, slug, "chapters");
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
                images.store(key, "api", fresh[i]); // same key shape call_cached persists under
            }
            crow::response r{R"({"ok":true})"};
            r.set_header("Content-Type", "application/json");
            return r;
        })};
    });

    // Reader prefetch input (image-cache): remember the position -- it is the
    // anchor the skip-rerun reads back -- and report the window. No worker
    // here; task 3.3 consumes the returned page list.
    CROW_ROUTE(app, "/reader/position")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            std::string chapter, manga, plugin;
            if (body) {
                chapter = str_field(body, "chapter");
                manga = str_field(body, "manga");
                plugin = str_field(body, "plugin");
            }
            if (chapter.empty() || manga.empty() || plugin.empty()) {
                crow::response r{R"({"ok":false,"error":"chapter, manga and plugin are required"})"};
                r.code = 400;
                r.set_header("Content-Type", "application/json");
                return r;
            }
            const reader::Position pos{chapter, static_cast<int>(std::max(1L, num_field(body, "page", 1)))};

            // last spot wins: single-user reader, one live anchor
            crow::json::wvalue pos_json;
            pos_json["plugin"] = plugin;
            pos_json["manga"] = manga;
            pos_json["chapter"] = chapter;
            pos_json["page"] = pos.page;
            const std::string pos_value = pos_json.dump();
            on_pool(db, [&] {
                store.state_set(kStateHost, kPositionKey, pos_value);
                return 0;
            });

            const std::vector<std::string> found =
                reader_window(store, db, plugin_pool, plugins, images, plugin, manga, pos);
            std::vector<crow::json::wvalue> window;
            for (const auto& url : found)
                window.emplace_back(url);
            if (!found.empty())
                prefetch.nudge(found); // background delta fetch (3.3)

            crow::json::wvalue out;
            out["window"] = std::move(window);
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
                if (t == crow::json::type::True || t == crow::json::type::False)
                    skipped = body["skipped"].b();
            }

            on_pool(db, [&] {
                store.state_set(kStateHost, std::string(kSkipPrefix) + id, skipped ? "1" : "0");
                return 0;
            });

            std::vector<crow::json::wvalue> window;
            if (const std::optional<std::string> stored =
                    on_pool(db, [&] { return store.state_get(kStateHost, kPositionKey); })) {
                if (const auto pos = crow::json::load(*stored)) {
                    const std::string plugin = str_field(pos, "plugin");
                    const std::string manga = str_field(pos, "manga");
                    const std::string chapter = str_field(pos, "chapter");
                    if (!plugin.empty() && !manga.empty() && !chapter.empty()) {
                        const reader::Position where{chapter,
                                                     static_cast<int>(std::max(1L, num_field(pos, "page", 1)))};
                        const std::vector<std::string> found =
                            reader_window(store, db, plugin_pool, plugins, images, plugin, manga, where);
                        for (const auto& url : found)
                            window.emplace_back(url);
                        if (!found.empty())
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

    // Serve-through image cache: hit = disk, miss+online = fetch+store+serve,
    // miss+offline = 404. Mime from the stored file's extension.
    CROW_ROUTE(app, "/cache/img")
    ([&](const crow::request& req) {
        const ImgReply r = serve_cached_image(images, param(req, "url"));
        crow::response res;
        res.code = r.code;
        res.body = std::move(r.body);
        res.set_header("Content-Type", std::string(r.mime));
        return res;
    });

    // CBZ export (cache-layout D6): stream the cached tree as an in-memory zip;
    // empty/unknown slug = empty-but-valid zip, never 404.
    CROW_ROUTE(app, "/export/cbz")
    ([](const crow::request& req) {
        crow::response res;
        const auto zip = cbz::build("data/cache", param(req, "plugin"), param(req, "slug"));
        if (!zip) {
            res.code = 500;
            res.set_header("Content-Type", "text/plain");
            res.body = "export failed";
            return res;
        }
        res.body = std::move(*zip);
        res.set_header("Content-Type", "application/zip");
        res.set_header("Content-Disposition",
                       "attachment; filename=\"" + param(req, "slug") + ".cbz\"");
        return res;
    });
}

} // namespace http
