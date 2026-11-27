#pragma once

/** \file pages_source.hpp
 *  \brief Pure decision core of GET /api/pages (pages-local-first D1/D2): which
 *  release answers a request and whether its page list comes from the
 *  `chapter_pages` rows (no plugin call) or from a fresh parse. The crow route
 *  injects the two lookups, so tests drive it without HTTP, store or plugin
 *  (test_pages, same seam as build_pages_response).
 */

#include <functional>
#include <string>
#include <vector>

namespace http::api {

/** \brief `error` string of a plugin envelope, for propagating the real
 *  upstream reason instead of the generic "unknown chapter" 404. */
std::string envelope_error(const std::string& envelope);

/** \brief Target release id: the `ch_id` query param when it matches a list
 *  entry, else the number match with ties to the id-min duplicate (the entry
 *  pages_api sorts first, so neighbors and fallback agree). */
std::string resolve_chapter_id(const std::string& chapters_envelope, const std::string& wanted, double chapter_number);

/** \brief Other releases with the same number, id-ascending, primary excluded,
 *  capped at three: a release whose own parse flakes falls back across them. */
std::vector<std::string> sibling_ids(const std::string& chapters_envelope, double chapter_number,
                                     const std::string& primary_id);

/** \brief Page urls of an ok envelope in parse order (non-strings skipped). */
std::vector<std::string> pages_urls(const std::string& envelope);

/** Result of the local-first walk. `id` empty = no release has pages. */
struct PagesSource {
    std::string id;
    std::string envelope;
    bool from_rows = false; // true = chapter_pages answered, the plugin was not called
};

/** \brief Walk the target and its same-number siblings: the first release with
 *  stored rows answers from them; otherwise the first whose parse yields pages
 *  answers from that envelope. `stored(id)` = rows (empty = none), `parse(id)` =
 *  envelope. No rows anywhere and no successful parse = empty result. */
PagesSource pick_pages_source(const std::string& chapters_env, double chapter_number, const std::string& wanted,
                              const std::function<std::vector<std::string>(const std::string&)>& stored,
                              const std::function<std::string(const std::string&)>& parse);

} // namespace http::api
