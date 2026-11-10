#include "http.hpp"
#include "http_routes.hpp"

#include "reader/worker.hpp"
#include "reader/cover_worker.hpp"

#include <cstdio>
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
cache::Fetcher makeCurlFetcher(std::string /*proxy*/) {
    return [](const std::string& url) -> std::optional<std::string> {
        // Shell-safe single quoting for the URL.
        std::string quoted;
        for (const char c : url) {
            quoted += c == '\'' ? "'\\''" : std::string(1, c);
        }
        // -f: HTTP errors become non-zero exits, never cached bodies.
        FILE* pipe = popen(("curl -sfL --max-time 30 '" + quoted + "' 2>/dev/null").c_str(), "r");
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
           cache::Cache& images, int port, const std::string& ini_path) {
    crow::SimpleApp app;
    crow::mustache::set_base("templates");

    // Image-cache 3.3: one background prefetcher over the shared img cache;
    // woken only by the two reader endpoints below, stopped at scope exit.
    reader::Worker prefetch{images};
    // Cover prefetcher: separate worker from the page window; woken by the
    // library/grid render, cache-first (disk hit = no fetch).
    reader::CoverWorker covers{images};

    // Crow's built-in static handler already serves /static/<path> from ./static.

    http::register_pages(app, store, db, logs, covers);
    http::register_api(app, store, db, plugin_pool, plugins, images, prefetch, covers, ini_path);

    app.port(port).concurrency(4).run();
}
