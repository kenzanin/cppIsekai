#include "http.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "stale.hpp"

#include "config/config.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "reader/window.hpp"
#include "reader/worker.hpp"
#include "store/store.hpp"

#include <crow.h>
#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int kPageSize = 12;

std::string param(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return (value != nullptr) ? value : "";
}

int page_of(const std::string& text) {
    const int page = text.empty() ? 1 : std::atoi(text.c_str());
    return page < 1 ? 1 : page;
}

// Every database call runs on the single db worker, which owns the one connection.
// Same wrapper serves the plugin pool: one future hop, caller blocks.
template <class Fn> auto on_pool(BS::thread_pool<>& pool, Fn fn) -> decltype(fn()) {
    auto done = std::make_shared<std::promise<decltype(fn())>>();
    pool.detach_task([done, fn = std::move(fn)] {
        try {
            done->set_value(fn());
        } catch (...) {
            done->set_exception(std::current_exception()); // release the waiting future either way
        }
    });
    try {
        return done->get_future().get();
    } catch (const std::exception& e) {
        spdlog::error("on_pool failed: {}", e.what());
        throw;
    }
}

// Reserved plugin_state rows for host bookkeeping: per-chapter skip flags and
// the last reader position (the anchor the skip-rerun reads back).
constexpr const char* kStateHost = "host";
constexpr const char* kSkipPrefix = "host/skip/";
constexpr const char* kPositionKey = "host/pos";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.
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

// curl-impersonate ships symbol but no header on host (system curl headers
// predate it; see knowledge/references/dependency-api.md). Same as plugin.cpp.
extern "C" CURLcode curl_easy_impersonate(CURL* easy, const char* client, unsigned clientscope);

// The one production curl call for the image cache: plain GET, body on 2xx.
cache::Fetcher makeCurlFetcher(std::string proxy) {
    return [proxy = std::move(proxy)](const std::string& url) -> std::optional<std::string> {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(curl_easy_init(), &curl_easy_cleanup);
        if (!easy) {
            return std::nullopt;
        }

        std::string body;
        const auto setup = [&](CURL* e) {
            curl_easy_setopt(e, CURLOPT_URL, url.c_str());
            curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(
                e, CURLOPT_WRITEFUNCTION, +[](char* p, size_t n, size_t l, void* ud) {
                    static_cast<std::string*>(ud)->append(p, n * l);
                    return n * l;
                });
            curl_easy_setopt(e, CURLOPT_WRITEDATA, &body);
            plugin::applyProxy(e, proxy); // host-wide; empty = direct
            // ponytail: plugin-parity 30s / chrome116, no per-pool knobs
            // until a real need shows up.
            curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, 30000L);
            curl_easy_setopt(e, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
            curl_easy_impersonate(e, "chrome116", 1);
        };
        setup(easy.get());
        // ponytail: this impersonate build intermittently fails the TLS
        // handshake against MangaDex's LB; one fresh-handle retry removes it
        // (same workaround as the plugin fetch).
        CURLcode rc = CURLE_OK;
        for (int attempt = 0; attempt < 2; ++attempt) {
            rc = curl_easy_perform(easy.get());
            if (rc == CURLE_OK || rc != CURLE_SSL_CONNECT_ERROR) {
                break;
            }
            easy.reset(curl_easy_init());
            if (!easy) {
                return std::nullopt;
            }
            setup(easy.get());
        }
        if (rc != CURLE_OK) {
            return std::nullopt;
        }

        long code = 0;
        curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &code);
        if (code < 200 || code >= 300) {
            return std::nullopt; // error pages must not become cache entries
        }
        return body;
    };
}

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins, LogBuffer& logs,
           cache::Cache& images, int port, const std::string& ini_path) {
    crow::SimpleApp app;
    crow::mustache::set_base("templates");

    // Image-cache 3.3: one background prefetcher over the shared img cache;
    // woken only by the two reader endpoints below, stopped at scope exit.
    reader::Worker prefetch{images};

    // Crow's built-in static handler already serves /static/<path> from ./static.

    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.mustache", home_context(store, db, req)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.mustache", home_context(store, db, req)); });

    CROW_ROUTE(app, "/search")([&] { return html("search.mustache", layout("search")); });
    CROW_ROUTE(app, "/history")([] { return html("blank.mustache", layout("history")); });
    CROW_ROUTE(app, "/plugins")([&] { return html("plugins.mustache", layout("plugins")); });
    CROW_ROUTE(app, "/about")([] { return html("blank.mustache", layout("about")); });

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

    app.port(port).concurrency(4).run();
}
