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
    std::string cookie_jar;
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

/** \brief Targets the curl-impersonate pair accept: chrome/edge/safari from
 *  the linked lib, ff* from the firefox sibling (both probed, v0.6.1). */
inline bool is_known_target(std::string_view t) {
    for (std::string_view k :
         {"chrome99", "chrome100", "chrome101", "chrome104", "chrome107", "chrome110", "chrome116", "edge99",
          "edge101", "safari15_3", "safari15_5", "ff91esr", "ff95", "ff98", "ff100", "ff102", "ff109", "ff117"}) {
        if (t == k) {
            return true;
        }
    }
    return false;
}

/** \brief Which lib family serves the profile: ff*/firefox* run on the
 *  firefox sibling lib, everything else on the linked chrome family. */
enum class Family { Chrome, Firefox };

inline Family family_of(std::string_view profile) {
    if (profile.rfind("ff", 0) == 0 || profile.rfind("firefox", 0) == 0) {
        return Family::Firefox;
    }
    return Family::Chrome;
}

/** \brief Execute \p req; on SSL_CONNECT_ERROR retry once on a fresh handle
 *  (ponytail: the impersonate build drops the handshake against big LBs). */
Result sync(const Request& req, const std::string& proxy);

} // namespace fetch
