#pragma once

/** \file enrich_internal.hpp
 *  \brief Declarations for the enrichment helpers; definitions live in
 *  enrich_source.cpp / enrich_chosen.cpp / enrich_detail.cpp. Not public API. */

#include "api_ctx.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <glaze/glaze.hpp>

#include <cstdint>
#include <optional>

namespace http::api {

// -- enrich_source.cc --------------------------------------------------------
// Serve the per-source entry of the stored `fetched` array shaped as the
// enrichment envelope; nullopt when this source has nothing stored.
std::optional<crow::json::wvalue> stored_source(const std::string& fetched_json, const std::string& id,
                                                const std::string& kind, std::int64_t& out_fetched_at);

// GET /api/enrichment/<id>/<detail|covers>?plugin=&slug=&force=0|1
// cached-or-fetch / force per source. Never writes saved_manga / api_cache.
crow::response enrichment_source_response(plugin::Host& plugins, BS::thread_pool<>& db,
                                          BS::thread_pool<>& plugin_pool, Store& store, const std::string& id,
                                          const std::string& kind, const std::string& plugin,
                                          const std::string& slug, bool force);

// -- enrich_chosen.cc --------------------------------------------------------
// Validate + normalize the POST chosen member into a compact JSON string.
// nullopt on any shape violation (caller returns 400). Exact key whitelist;
// artist/author accept String or List<String> and normalize to array.
std::optional<std::string> normalize_chosen(const crow::json::rvalue& ch);

// Read a field from an already-parsed chosen object (batch callers avoid a
// second title_meta_get per field).
std::optional<std::string> chosen_field(const glz::generic& chosen, const char* key);

// Chosen display title from title_meta.chosen; nullopt when unset/empty.
std::optional<std::string> chosen_title(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug);

// Chosen cover override from title_meta.chosen; nullopt when unset/empty.
std::optional<std::string> chosen_cover(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug);

// Merge title_meta.chosen over the scraper detail envelope (design D3):
//   chosen.<field> ?? source; tags = (source tags + chosen.tags_add) - chosen.tags_hidden;
//   cover override is also exposed as `chosen_cover`; `enrichment: {fetched_at, chosen}` rides
//   the top level for the Alpine picker. Corrupt/absent rows return the envelope untouched.
std::string apply_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                         const std::string& envelope);

// -- enrich_detail.cc --------------------------------------------------------
/** \brief D2: persist the resolved display fields of a detail envelope onto the saved
 *  row. Called from /api/detail after apply_chosen and from refresh_title when the
 *  detail method ran -- NEVER from apply_chosen itself (the enrichment guard test
 *  asserts candidates never touch saved_manga). Tags are the merged list the page
 *  renders; the display synopsis is `description` falling back to `synopsis`. */
void materialize_detail(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                        const std::string& slug, const std::string& resolved_env);

/** \brief D3/D7/D8: the row-served detail envelope for a saved manga -- row values
 *  plus the title_meta overlay (enrichment block + chosen_cover), flagged cached.
 *  Empty string when the row cannot serve (absent or nothing materialized). */
std::string saved_detail_response(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                  const std::string& slug);

/** \brief library-promote-on-save D2: promote the pair's cached detail/chapters
 *  envelopes onto local rows -- the exact helpers the detail route runs
 *  (apply_chosen -> materialize_detail, materialize_chapters, sync_saved_stats),
 *  zero plugin calls. Silent no-op when nothing is cached (lazy resolve-on-open
 *  stays the fallback). Called from POST /api/library after the upsert. */
void promote_saved_cached(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug);

} // namespace http::api
