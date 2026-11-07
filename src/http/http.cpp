#include "http.hpp"
#include "img.hpp"

#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <crow.h>
#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
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
cache::Fetcher makeCurlFetcher() {
    return [](const std::string& url) -> std::optional<std::string> {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(curl_easy_init(),
                                                                 &curl_easy_cleanup);
        if (!easy) {
            return std::nullopt;
        }

        std::string body;
        const auto setup = [&](CURL* e) {
            curl_easy_setopt(e, CURLOPT_URL, url.c_str());
            curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(
                e, CURLOPT_WRITEFUNCTION,
                +[](char* p, size_t n, size_t l, void* ud) {
                    static_cast<std::string*>(ud)->append(p, n * l);
                    return n * l;
                });
            curl_easy_setopt(e, CURLOPT_WRITEDATA, &body);
            // ponytail: plugin-parity 30s / chrome116, no per-pool knobs until
            // a real need shows up; proxy lands here in task 2.1.
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

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
           LogBuffer& logs, cache::Cache& images, int port) {
    crow::SimpleApp app;
    crow::mustache::set_base("templates");

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
        crow::response r{on_pool(plugin_pool, [&] { return plugins.call(id, action, p.dump()); })};
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
