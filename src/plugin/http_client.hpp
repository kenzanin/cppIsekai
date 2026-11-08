#pragma once

/** \file http_client.hpp
 *  \brief Interim synchronous curl client shared by the plugin runtimes.
 *
 * Moved verbatim out of the plugin crown (change source-split-clangdoc).
 * The change fetch-gate will relocate this unit to src/fetch/ and make the
 * impersonation profile / cookie policy dynamic; until then this remains the
 * one option block, plus its duplicated copy inside the lua http.fetch
 * closure (lua/lua_runtime.cpp).
 */

#include <curl/curl.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// curl-impersonate ships the symbol but no header on this host (system curl
// headers predate it; see knowledge/references/dependency-api.md).
extern "C" CURLcode curl_easy_impersonate(CURL* easy, const char* client, unsigned clientscope);

namespace plugin {

/** Sink for the transfer callbacks: body bytes plus lowercased headers. */
struct FetchSink {
    std::string body;
    std::map<std::string, std::string> headers; // keys lowercased, last wins
};

/** curl easy handle owner; cleanup bound at construction. */
using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

/** \brief Result of one synchronous fetch; \p error is empty on success. */
struct FetchResult {
    long status = 0; /**< final HTTP response code, 0 when the transfer failed */
    std::string body; /**< response body bytes */
    std::map<std::string, std::string> headers; /**< response headers, keys lowercased */
    std::string error; /**< curl diagnostic; empty on success */
};

/** \brief CURLOPT_WRITEFUNCTION body sink. */
size_t on_body(char* ptr, size_t size, size_t nmemb, void* ud);

/** \brief CURLOPT_HEADERFUNCTION sink; lowercases keys, last value wins. */
size_t on_header(char* ptr, size_t size, size_t nmemb, void* ud);

/** \brief The one synchronous fetch path: lua http.fetch and the wasm
 *  env.http_fetch import both land here. Options pre-extracted by the caller;
 *  \p error empty on success. */
FetchResult fetch_sync(const std::string& url, long timeout_ms, const std::string& profile, const std::string& method,
                       const std::string& body, const std::string& accept_header, const std::string& ua_header,
                       const std::vector<std::pair<std::string, std::string>>& extra_headers,
                       const std::string& proxy);

} // namespace plugin
