#pragma once

/** \file fetch.hpp
 *  \brief The single synchronous fetch gate: one option block, one
 *  curl_easy_impersonate call site, the ponytail retry, and the host-wide
 *  proxy rule (change fetch-gate; machinery moved verbatim from
 *  plugin/http_client.cpp and the image-cache fetcher).
 *
 * Profile resolution (opts > ini > live default) lands in task 1.2; until
 * then every caller passes the profile explicitly and an empty profile
 * falls back to chrome116, the pre-gate pinned value. */

#include <algorithm>
#include <array>
#include <cstdint>
#include <curl/curl.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fetch {

/** \brief One outbound request. Empty profile => the gate fallback; the
 *  cookie_jar field is consumed by task 2.1. */
struct Request {
    std::string url;
    long timeout_ms = 30000;
    std::string profile;
    std::string method = "GET";
    std::string body;
    std::string accept_header;
    std::string ua_header;
    std::vector<std::pair<std::string, std::string>> headers;
    // 2.1/2.2: per-plugin jar path; the runtime opts opt in via a cookie flag.
    std::string cookie_jar;
    // true: skip impersonation entirely (stock curl). Image-cache fetcher
    // uses this -- the chrome116 fingerprint triggers MangaDex CDN's
    // anti-bot challenge on uploads.mangadex.org (plain curl gets the JPEG).
    bool plain = false;
};

/** \brief Transfer outcome; error non-empty means no response at all. */
struct Result {
    long status = 0;
    std::string body;
    std::map<std::string, std::string> headers; // keys lowercased, last wins
    std::string error;
};

/** \brief One proxy rule for every handle (plugin fetch + image getter):
 *  empty string means direct, no CURLOPT_PROXY set. */
inline void applyProxy(CURL* e, const std::string& proxy) {
    if (!proxy.empty()) {
        curl_easy_setopt(e, CURLOPT_PROXY, proxy.c_str());
    }
}

/** \brief Loopback-url check for the proxy exemption (change browser-host D6):
 *  the local MCP daemon must never ride the host-wide proxy. Host = whatever
 *  sits between scheme:// and the first /?#, v6 brackets kept then stripped,
 *  port ignored. */
inline bool is_loopback_url(std::string_view url) {
    const auto scheme = url.find("://");
    if (scheme == std::string_view::npos) {
        return false;
    }
    url.remove_prefix(scheme + 3);
    const auto path = url.find_first_of("/?#");
    if (path != std::string_view::npos) {
        url = url.substr(0, path);
    }
    if (!url.empty() && url.front() == '[') { // [::1]:8890
        const auto close = url.find(']');
        if (close != std::string_view::npos) {
            url = url.substr(1, close - 1);
        }
    } else {
        const auto colon = url.find(':');
        if (colon != std::string_view::npos) {
            url = url.substr(0, colon);
        }
    }
    return url == "127.0.0.1" || url == "::1" || url == "localhost";
}

/** \brief Targets the curl-impersonate pair accept: chrome/edge/safari from
 *  the linked lib, ff* from the firefox sibling (both probed, v0.6.1). */
inline bool is_known_target(std::string_view t) {
    static constexpr std::array<std::string_view, 18> kTargets{
        "chrome99", "chrome100", "chrome101", "chrome104", "chrome107", "chrome110", "chrome116",
        "edge99",   "edge101",   "safari15_3", "safari15_5", "ff153",    "ff154",     "ff155",
        "firefox153", "firefox154", "firefox155", "tor145"}; // ff/tor: system OpenSSL (NSS vs BoringSSL)
    return std::ranges::any_of(kTargets, [&](std::string_view k) { return t == k; });
}

/** \brief Which lib family serves the profile. Firefox is unsupported: the
 *  ff build (NSS) cannot be loaded alongside the linked chrome build (BoringSSL)
 *  in one process; ff* falls back to the chrome engine. */
enum class Family : std::uint8_t { Chrome, Firefox };

inline Family family_of(std::string_view profile) {
    if (profile.starts_with("ff") || profile.starts_with("firefox")) {
        return Family::Firefox;
    }
    return Family::Chrome;
}

/** \brief Execute \p req; on SSL_CONNECT_ERROR retry once on a fresh handle
 *  (ponytail: the impersonate build drops the handshake against big LBs). */
Result sync(const Request& req, const std::string& proxy);

} // namespace fetch
