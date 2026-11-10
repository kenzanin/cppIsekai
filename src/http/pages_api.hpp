#pragma once

/** \file pages_api.hpp
 *  \brief Pure core of GET /api/pages (reader-page D1): shapes the reader
 *  response from the two stale-layer envelopes. The crow route in
 *  http_api.cpp stale-serves `chapters` + `pages` and hands both envelopes
 *  here; tests fake the plugin with canned envelopes (test_pages), the same
 *  seam as test_stale. Crow stays out of this header on purpose.
 */

#include <optional>
#include <string>

/** Path context of one /api/pages request. */
struct PagesCtx {
    std::string plugin;
    std::string slug;
    int ch{};
};

/** Reader page-list JSON from the chapters envelope and the target chapter's
 *  pages envelope. nullopt = unknown chapter or unusable envelope (the route
 *  answers 404). Page entries are host-rendered /cache/img URLs -- clients
 *  never assemble cache paths themselves (ratio suffixes, v3 rel_path and
 *  host-side transforms stay host-owned, design D1).
 */
std::optional<std::string> build_pages_response(const std::string& chs_env, const std::string& pages_env,
                                                const PagesCtx& ctx);
