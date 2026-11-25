#include "http.hpp"
#include "http_routes.hpp"

#include "fetch/fetch.hpp"
#include "reader/cover_worker.hpp"
#include "reader/worker.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

// The image-cache fetcher: a curl CLI subprocess. ponytail: the host process
// already carries the impersonate libcurl; loading a second libcurl TLS stack
// in-process segfaults on first use, and the impersonate stacks themselves
// die with SSL_CONNECT_ERROR against uploads.mangadex.org's CDN. The system
// CLI (OpenSSL) is the only engine proven to handshake there; promote to a
// dedicated out-of-process fetch helper if anything heavier than images
// ever needs it.
cache::Fetcher makeCurlFetcher(const std::string& proxy, const plugin::Host& plugins) {
    return [&plugins, proxy](const std::string& url, const std::string& plugin) -> std::optional<std::string> {
        // Manifest `image_engine` opt-in: CF-fronted image CDNs need the
        // chrome impersonation fingerprint; stock curl eats a 403 challenge.
        if (const std::string engine = plugins.image_engine(plugin); !engine.empty()) {
            fetch::Request rq;
            rq.url = url;
            rq.profile = engine; // timeout_ms defaults to 30 s
            // CF challenge zones mint httpOnly cookies in the host browser;
            // the image fetch is curl-side, so feed it the plugin jar when one
            // exists (seeded by the plugin's cookie=true calls / harvest).
            const std::string jar = "data/plugin-state/" + plugin + ".cookies.txt";
            if (std::error_code ec; std::filesystem::exists(jar, ec) && !ec) {
                rq.cookie_jar = jar;
            }
            // cf_clearance is bound to the UA that solved the challenge; echo it
            if (const std::string ua = plugins.image_ua(plugin); !ua.empty()) {
                rq.ua_header = ua;
            }
            if (const std::string referer = plugins.image_referer_for_url(url, plugin);
                !referer.empty() && referer != "none") { // "none": sentinel, no header (D-sentinel)
                rq.headers = {{"Referer", referer}};
            }
            const fetch::Result r = fetch::sync(rq, proxy);
            if (r.error.empty() && r.status >= 200 && r.status < 300) {
                return r.body;
            }
            return std::nullopt;
        }
        // Shell-safe single-quote for argv fragments (URL and Referer alike).
        const auto quote = [](const std::string& s) {
            std::string out;
            for (const char c : s) {
                out += c == '\'' ? "'\\''" : std::string(1, c);
            }
            return out;
        };
        // -f: HTTP errors become non-zero exits, never cached bodies.
        // Per-site image Referer, data-driven (js-image-referer-gap): the
        // plugin's manifest `image_referer` override, else `website` origin.
        std::string referer;
        if (!plugin.empty()) {
            if (const std::string ref = plugins.image_referer_for_url(url, plugin); !ref.empty() && ref != "none") {
                referer = " -e '" + quote(ref) + "'";
            }
        }
        FILE* pipe = popen(("curl -sfL --max-time 30" + referer + " '" + quote(url) + "' 2>/dev/null").c_str(), "r");
        if (!pipe) {
            return std::nullopt;
        }
        std::string body;
        char buf[65536];
        size_t n = 0;
        while ((n = fread(buf, 1, sizeof buf, pipe)) > 0) {
            body.append(buf, n);
        }
        if (pclose(pipe) != 0) {
            return std::nullopt;
        }
        return body;
    };
}

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins, LogBuffer& logs,
           cache::Cache& images, int port, const std::string& ini_path, const config::Overrides& overrides,
           const std::string& templates_dir) {
    crow::SimpleApp app;
    // ponytail: crow deflate/gzip on dynamic text bodies (HTML/JSON/WS-less);
    // static .br assets set res.compressed=false themselves via br_asset, and
    // binary/image/zip bodies opt out per-route. Compression of already-
    // compressed bytes would waste CPU and bloat.
    app.use_compression(crow::compression::GZIP);
    crow::mustache::set_base(templates_dir);

    // Image-cache 3.3: one background prefetcher over the shared img cache;
    // woken only by the two reader endpoints below, stopped at scope exit.
    reader::Worker prefetch{images};
    // Cover prefetcher: separate worker from the page window; woken by the
    // library/grid render, cache-first (disk hit = no fetch).
    reader::CoverWorker covers{images};

    // Static: prefer precompressed .br (vendored js); fall back to crow's
    // built-in static handler for anything we don't handle here.
    http::register_static(app);

    http::register_pages(app, store, db, logs, covers);
    http::register_api(app, store, db, plugin_pool, plugins, images, prefetch, covers, ini_path, overrides);

    app.port(port).concurrency(4).run();
}
