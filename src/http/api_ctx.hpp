#pragma once

/** \file api_ctx.hpp
 *  \brief Internal shared context + helpers for the http_api route TUs.
 *  Monolith split (compile parallelism, <=~200 loc per unit): one Ctx of
 *  service refs handed to every register_*; state keys, body field readers
 *  and the title-pair guard live here once. Not a public http surface.
 */

#include "http_routes.hpp"
#include "stale.hpp"

#include "config/config.hpp"
#include "store/store.hpp"

#include "position_api.hpp"
#include "reader/window.hpp"

#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace http::api {

/** Service handles every route group needs; built once in register_api. */
struct Ctx {
    crow::SimpleApp& app;
    Store& store;
    BS::thread_pool<>& db;
    BS::thread_pool<>& plugin_pool;
    plugin::Host& plugins;
    cache::Cache& images;
    reader::Worker& prefetch;
    reader::CoverWorker& covers;
    const std::string& ini_path;
    const config::Overrides& overrides;
};

// Reserved plugin_state rows for host bookkeeping: the last reader position
// (the anchor the skip-rerun reads back) and per-pair detail flags.
inline constexpr const char* kStateHost = "host";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.
inline std::string str_field(const crow::json::rvalue& j, const char* key) {
    return j.has(key) && j[key].t() == crow::json::type::String ? std::string(j[key].s()) : std::string{};
}
inline long num_field(const crow::json::rvalue& j, const char* key, long def) {
    return j.has(key) && j[key].t() == crow::json::type::Number ? j[key].i() : def;
}

// manga-detail 1.1: the {plugin, slug} path pair guard. Slugs are our own
// cache-layout keys; the whitelist used to be [a-zA-Z0-9-_.] but real-world slugs
// carry unicode/apostrophes/exclamation marks. The guard now denies only
// path-significant or control bytes and dot segments: `/` `\` `?` `#` `%`,
// space, C0/DEL controls; `..` is always refused. Returns false when the pair
// needs the not-found response.
inline bool valid_title_pair(const std::string& plugin, const std::string& slug) {
    const auto ok = [](const std::string& s) {
        // The slug IS the site-minted id (4ucomics: 202 chars) and doubles as the
        // per-title cache dir component, so the ceiling is the filesystem's
        // NAME_MAX, not a display length.
        if (s.empty() || s.size() > 255) {
            return false;
        }
        for (const unsigned char c : s) {
            if (c == '/' || c == '\\' || c == '?' || c == '#' || c == '%' || c == 0x20 || c < 0x20 || c == 0x7f) {
                return false;
            }
        }
        if (s.contains("..")) {
            return false;
        }
        return true;
    };
    return ok(plugin) && ok(slug);
}

// The reader window composition (image-cache): chapter list + pages through
// the cached ABI path (stale-serve included), skip flags from plugin_state,
// math in reader::window_parts. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
// Defined in http_api_reader.cpp; the state TU reuses it for the skip-rerun.
// Per-chapter parts: the prefetch nudge needs them; flatten for the response.
std::vector<reader::WindowPart> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                              plugin::Host& plugins, const std::string& plugin_id,
                                              const std::string& manga_id, const reader::Position& pos);

// Route-group registration, one TU each (http_api_*.cpp).
void register_admin(Ctx& /*c*/); // plugin registry/admin + config endpoints
void register_library(Ctx& /*c*/); // saved-library CRUD
void register_title(Ctx& /*c*/);   // detail/related/chapters/refresh envelopes
void register_reader(Ctx& /*c*/); // reader window + position routes
void register_state(Ctx& /*c*/); // chapter skip/read/state flags
void register_content(Ctx& /*c*/); // reader pages, image cache, cbz export
void register_enrich(Ctx& /*c*/); // enrichment fetch/chosen/reset

// manga-detail-table D2: forward decls so refresh_title (below, parsed before or
// after enrich_internal depending on the TU include order) links to the
// definitions in enrich_internal.hpp.
void materialize_detail(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                        const std::string& resolved_env);
std::string apply_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                         const std::string& envelope);

} // namespace http::api

// Title-state domain layer (stats sync / refresh driver) lives in
// src/titlestate.hpp -- pulled in for every api TU; update/scheduler uses it
// without the http layer.
#include "titlestate.hpp"
