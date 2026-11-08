#include "http.hpp"
#include "http_routes.hpp"

#include "fetch/fetch.hpp"
#include "reader/worker.hpp"

#include <string>
#include <utility>

// The image-cache fetcher rides the same gate as the plugins (one
// curl_easy_impersonate call site); the profile stays pinned chrome116 here --
// task 1.2's live default is a plugin-path concern.
cache::Fetcher makeCurlFetcher(std::string proxy) {
    return [proxy = std::move(proxy)](const std::string& url) -> std::optional<std::string> {
        fetch::Request req;
        req.url = url;
        req.profile = "chrome116"; // ponytail: plugin-parity 30s, no per-pool knobs
        const fetch::Result fr = fetch::sync(req, proxy);
        if (!fr.error.empty() || fr.status < 200 || fr.status >= 300) {
            return std::nullopt; // error pages must not become cache entries
        }
        return fr.body;
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

    http::register_pages(app, store, db, logs);
    http::register_api(app, store, db, plugin_pool, plugins, images, prefetch, ini_path);

    app.port(port).concurrency(4).run();
}
