#pragma once

/** \file http_client.hpp
 *  \brief Plugin-side seam over the fetch gate: the runtimes keep the
 *  positional signature, the gate owns every curl detail (change fetch-gate
 *  task 1.1; the machinery moved verbatim to src/fetch/). */

#include "fetch/fetch.hpp"

#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

namespace plugin {

/** \brief The gate result, unchanged for the runtimes. */
using FetchResult = fetch::Result;

/** \brief Positional adapter: assembles a fetch::Request and runs it. The
 *  profile threads straight through; task 1.2 adds the live default there. */
FetchResult fetch_sync(const std::string& url, long timeout_ms, const std::string& profile, const std::string& method,
                       const std::string& body, const std::string& accept_header, const std::string& ua_header,
                       const std::vector<std::pair<std::string, std::string>>& extra_headers,
                       const std::string& proxy);

/** \brief Unknown-target guard (task 1.2): empty => live default; a target
 *  the linked libs do not know => warn and fall back (the request runs). */
inline std::string checked_profile(std::string want, const std::string& live_default) {
    if (want.empty()) {
        want = live_default;
    }
    if (want.empty()) {
        want = "chrome116";
    }
    if (!fetch::is_known_target(want)) {
        spdlog::warn("[fetch] unknown profile '{}' -> using '{}'", want, live_default);
        want = live_default.empty() ? "chrome116" : live_default;
    }
    return want;
}

/** \brief The live default-profile cell type itself is plugin::LiveProfile
 *  (plugin.hpp); the runtimes receive it from the Host. */

} // namespace plugin
