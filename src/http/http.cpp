#include "http.hpp"
#include "http_routes.hpp"

#include "reader/worker.hpp"

#include <curl/curl.h>

#include <memory>
#include <string>
#include <utility>

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

    http::register_pages(app, store, db, logs);
    http::register_api(app, store, db, plugin_pool, plugins, images, prefetch, ini_path);

    app.port(port).concurrency(4).run();
}
