#include "http_routes.hpp"

#include "config/config.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"
#include "position_api.hpp"
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
#include <ctime>

namespace http {
namespace {

// Reserved plugin_state rows for host bookkeeping: per-chapter skip flags and
// the last reader position (the anchor the skip-rerun reads back).
constexpr const char* kStateHost = "host";
constexpr const char* kSkipPrefix = "host/skip/";
constexpr const char* kReadPrefix = "host/read/";
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
        return call_cached(store, plugin, method, params, [&] { return plugins.call(plugin, method, params); });
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
        return call_cached(store, plugin_id, "chapters", chapters_query,
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
            return call_cached(store, plugin_id, "pages", pages_query,
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
                  plugin::Host& plugins, cache::Cache& images, reader::Worker& prefetch, reader::CoverWorker& covers,
                  const std::string& ini_path, const config::Overrides& overrides_ref) {
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
                            const auto c = obj.find("cover");
                            if (c == obj.end()) continue;
                            const auto* s = std::get_if<std::string>(&c->second.data);
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

    // library-save 2.1: JSON routes backing the saved-manga Library grid
    // and the detail-page toggle. POST-only writes (codebase convention);
    // GET is membership for the toggle and the grid source.
    CROW_ROUTE(app, "/api/library")
    ([&] {
        const auto rows = on_pool(db, [&] { return store.saved_list(); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& r : rows) {
            crow::json::wvalue row;
            row["plugin"] = r.plugin;
            row["slug"] = r.slug;
            row["name"] = r.name;
            row["cover"] = r.cover;
            row["added_at"] = static_cast<double>(r.added_at);
            list[idx++] = std::move(row);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["data"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/library").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        const std::string body = req.body;
        const auto json = crow::json::load(body);
        if (!json || !json.has("plugin") || !json.has("slug"))
            return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
        SavedManga m;
        m.plugin = json["plugin"].s();
        m.slug = json["slug"].s();
        m.name = json.has("name") && json["name"].t() == crow::json::type::String ? json["name"].s() : std::string{json["slug"].s()};
        m.cover = json.has("cover") && json["cover"].t() == crow::json::type::String ? json["cover"].s() : std::string{};
        on_pool(db, [&] { store.save_manga(m); return 0; });
        crow::response r{R"({"ok":true,"saved":true})"};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/library/remove").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        const auto json = crow::json::load(req.body);
        if (!json || !json.has("plugin") || !json.has("slug"))
            return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
        const std::string plugin = json["plugin"].s();
        const std::string slug = json["slug"].s();
        on_pool(db, [&] { store.unsave_manga(plugin, slug); return 0; });
        crow::response r{R"({"ok":true,"saved":false})"};
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

    // detail-enrichment 2.1: related readings; same envelope and stale-serve
    // rules as /api/detail, plugin method "related".
    CROW_ROUTE(app, "/api/related/<string>/<string>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug) {
        return title_data_response(plugins, images, db, plugin_pool, store, plugin, slug, "related");
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

    // reader-page 2.2: continue-hint data. db worker (store.hpp ponytail);
    // absent position = 404 so the detail badge stays hidden.
    CROW_ROUTE(app, "/api/position/<string>/<string>")
    ([&](const crow::request&, const std::string& plugin, const std::string& manga) -> crow::response {
        const auto pos = on_pool(db, [&] { return store.position_get(plugin, manga); });
        if (!pos) {
            crow::response r{R"({"ok":false})"};
            r.code = 404;
            r.set_header("Content-Type", "application/json");
            return r;
        }
        crow::json::wvalue out;
        out["ch"] = pos->chapter_number;
        out["page"] = pos->page;
        out["mode"] = pos->mode;
        out["direction"] = pos->direction;
        out["zoom"] = pos->zoom;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
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

            // reader-page 2.2: optional mode/direction -- absent keeps the stored
            // value (the same db hop stamps updated_at and stores the result).
            std::string mode, direction, zoom;
            if (body) {
                mode = str_field(body, "mode");
                direction = str_field(body, "direction");
                zoom = str_field(body, "zoom");
            }
            // The chapter NUMBER rides along (the reader knows it from ?ch/the
            // pages list); absent keeps the stored one. Feeds the resume hint
            // and the prefetch worker's v3 placement.
            // ch 0 is valid; absence is signaled by the optional, not by 0.
            std::optional<double> chapter_number;
            if (body && body.has("chapter_number") && body["chapter_number"].t() == crow::json::type::Number)
                chapter_number = body["chapter_number"].d();
            on_pool(db, [&] {
                const auto stored = store.position_get(plugin, manga);
                ReaderPosition next{plugin, manga, chapter, chapter_number.value_or(0), pos.page, mode, direction, zoom, 0};
                if (stored) {
                    if (!chapter_number) // absent: keep the stored number (0 included)
                        next.chapter_number = stored->chapter_number;
                    // same chapter, no page sent -> keep the page
                    if (chapter == stored->chapter_id)
                        next.page = pos.page > 1 ? pos.page : stored->page;
                }
                store.position_set(merge_position(stored, next));
                // detail-enrichment 1.1 (chapter-list-controls): max pages-read per chapter for the row counter/check.
                if (!chapter.empty()) {
                    int prev = 0;
                    if (const auto s = store.state_get(kStateHost, std::string(kReadPrefix) + chapter); s && !s->empty())
                        prev = std::atoi(s->c_str()); // state_get returns the stored "N"
                    const int p = pos.page > 1 ? pos.page : 1;
                    const int now = std::max(prev, p);
                    store.state_set(kStateHost, std::string(kReadPrefix) + chapter, std::to_string(now));
                }
                spdlog::debug("position {} {} ch={} p={} cn={} mode='{}' dir='{}'", plugin, manga, chapter,
                              next.page, next.chapter_number, next.mode, next.direction);
                return 0;
            });

            const std::vector<std::string> found =
                reader_window(store, db, plugin_pool, plugins, images, plugin, manga, pos);
            std::vector<crow::json::wvalue> window;
            for (const auto& url : found)
                window.emplace_back(url);
            if (!found.empty()) {
                if (chapter_number) { // v3 placement; else the sha fallback
                    spdlog::debug("nudge {} urls, ctx {}.{} ch{} base-p{}", found.size(), plugin, manga,
                                  *chapter_number, pos.page + 1);
                    prefetch.nudge(found, {plugin, manga, int(*chapter_number), pos.page + 1});
                } else {
                    spdlog::debug("nudge {} urls, no chapter number -> sha fallback", found.size());
                    prefetch.nudge(found); // background delta fetch (3.3)
                }
            }

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
            const std::optional<ReaderPosition> last =
                on_pool(db, [&] { return store.position_last(); });
            if (last) {
                const std::string& plugin = last->plugin;
                const std::string& manga = last->slug;
                const std::string& chapter = last->chapter_id;
                if (!plugin.empty() && !manga.empty() && !chapter.empty()) {
                    const reader::Position where{chapter, last->page};
                    const std::vector<std::string> found =
                        reader_window(store, db, plugin_pool, plugins, images, plugin, manga, where);
                    for (const auto& url : found)
                        window.emplace_back(url);
                    if (!found.empty())
                        prefetch.nudge(found); // rerun delta (3.3)
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
    CROW_ROUTE(app, "/chapter/state")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            std::vector<std::string> ids;
            if (body && body.has("ids") && body["ids"].t() == crow::json::type::List)
                for (const auto& v : body["ids"])
                    if (v.t() == crow::json::type::String)
                        ids.push_back(v.s());
            crow::json::wvalue out;
            on_pool(db, [&] {
                for (const auto& id : ids) {
                    if (const auto s = store.state_get(kStateHost, std::string(kSkipPrefix) + id); s && *s == "1")
                        out[id]["skip"] = true;
                    if (const auto r = store.state_get(kStateHost, std::string(kReadPrefix) + id); r && !r->empty() && *r != "0")
                        out[id]["read"] = std::atoi(r->c_str());
                }
                return 0;
            });
            crow::response res{out};
            res.set_header("Content-Type", "application/json");
            return res;
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

// reader-page D1: ordered page URL list for the reader UI. Pages through
// the stale layer; prev/next chapter resolved from the chapter list by
// number so the reader can walk chapters without refetching the whole
// list. Each entry is a host-rendered /cache/img URL (client never
// builds cache paths -- design D1: ratio suffixes, v3 rel_path and
// host-side transforms stay host-owned).
    CROW_ROUTE(app, "/api/pages/<string>/<string>/<double>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug, double chd) -> crow::response {
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        const bool enabled = on_pool(db, [&] { return store.plugin_enabled(plugin); });
        if (!enabled) {
            crow::response r{R"({"ok":false,"error":"plugin disabled"})"};
            r.set_header("Content-Type", "application/json");
            return r;
        }
        crow::json::wvalue q;
        q["id"] = slug;
        const std::string chs_params = q.dump();
        const std::string body = on_pool(plugin_pool, [&] {
            const std::string chs_env =
                call_cached(store, plugin, "chapters", chs_params,
                            [&] { return plugins.call(plugin, "chapters", chs_params); });
            // Pre-resolve the chapter id from the list so the pages call
            // targets the right one even when the plugin's list is stale.
            std::string chapter_id;
            if (const auto chs = envelope_data(chs_env)) {
                for (const auto& item : *chs) {
                    if (!item.is_object())
                        continue;
                    const auto& entry = item.get_object();
                    const auto num = entry.find("number");
                    if (num == entry.end())
                        continue;
                    const auto* d = std::get_if<double>(&num->second.data);
                    if (!d || *d != chd)
                        continue;
                    const auto idf = entry.find("id");
                    if (idf != entry.end()) {
                        if (const auto* s = std::get_if<std::string>(&idf->second.data)) {
                            chapter_id = *s;
                            break;
                        }
                    }
                }
            }
            if (chapter_id.empty())
                return std::string{};
            crow::json::wvalue pages_q;
            pages_q["id"] = chapter_id;
            const std::string pages_params = pages_q.dump();
            const std::string pages_env =
                call_cached(store, plugin, "pages", pages_params,
                            [&] { return plugins.call(plugin, "pages", pages_params); });
            return build_pages_response(chs_env, pages_env, PagesCtx{plugin, slug, chd}).value_or(std::string{});
        });
        crow::response r;
        if (body.empty()) {
            r.code = 404;
            r.body = R"({"ok":false,"error":"unknown chapter"})";
        } else {
            r.body = std::move(body);
        }
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Serve-through image cache: hit = disk, miss+online = fetch+store+serve,
    // miss+offline = 404. Mime from the stored file's extension.
    // V3 (cache-layout-v3 D6): optional context params (plugin, slug, ch,
    // page, cover) drive the layout-specific landing path; a client-supplied
    // ratio is ignored — host sidecar is the source of truth (D3).
    CROW_ROUTE(app, "/cache/img")
    ([&](const crow::request& req) {
        const auto url = param(req, "url");
        const bool cover = param(req, "cover") == "1" || param(req, "cover") == "true";
        const auto plugin = param(req, "plugin");
        const auto slug = param(req, "slug");
        const int ch = std::atoi(param(req, "ch").c_str());
        const int page = std::atoi(param(req, "page").c_str());
        cache::Cache::CoverCtx cctx;
        cache::Cache::PageCtx pctx;
        if (cover) {
            cctx.cover = true;
            cctx.plugin = plugin;
            cctx.url = url;
        } else if (!plugin.empty() && !slug.empty() && ch >= 1 && page >= 1) {
            pctx = {plugin, slug, ch, page};
        }
        const ImgReply r = serve_cached_image(images, url, cctx, pctx);
        crow::response res;
        res.code = r.code;
        res.body = std::move(r.body);
        res.set_header("Content-Type", std::string(r.mime));
        if (r.ratio_header) res.set_header("X-Image-Ratio", *r.ratio_header);
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
