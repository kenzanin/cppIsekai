#pragma once

/** \file stale.hpp
 *  \brief Stale-serve / fresh-serve layer (search-cache design D3): ONE wrap
 *  around the ABI call route, no per-plugin branches.
 *
 *  Opt-in read-only methods. Fresh row in `api_cache` (JSONB, store-gated)
 *  is served without touching the plugin; on plugin success the envelope is
 *  upserted as JSONB; on failure the last-good row is served with
 *  `"cached":true` on top level, merged through glaze -- never
 *  string-spliced. The call itself is injected (any callable), so tests fake
 *  it hermetically. */

#include "cache/sha1.hpp"
#include "store/store.hpp"

#include <glaze/glaze.hpp>

#include <ctime>
#include <optional>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

/** Constant per-method TTL for the api_cache read model; ponytail: knob later. */
inline constexpr std::int64_t kApiCacheTtl = 24 * 3600;

/** \brief Read-only methods allowed to serve from api_cache (exact opt-in
 *  list; all of them, not just search). */
inline bool stale_optin(const std::string& method) {
    return method == "search" || method == "detail" || method == "chapters" ||
           method == "pages" || method == "genres";
}

/** \brief `ok` of an ABI envelope; anything but a literal true is a failure
 *  to serve. */
inline bool envelope_ok(const glz::generic& g) {
    const auto* obj = std::get_if<glz::generic::object_t>(&g.data);
    if (!obj) return false;
    const auto ok = obj->find("ok");
    const auto* b = ok == obj->end() ? nullptr : std::get_if<bool>(&ok->second.data);
    return b && *b;
}

/** \brief `data` of an ok ABI envelope, copied; nullopt on any malformed
 *  shape. Consumers: the reader window composition and the prefetch worker. */
inline std::optional<glz::generic::array_t> envelope_data(const std::string& envelope) {
    glz::generic parsed;
    if (glz::read_json(parsed, envelope) || !envelope_ok(parsed) || !parsed.is_object())
        return std::nullopt;
    const auto data = parsed.get_object().find("data");
    if (data == parsed.get_object().end() || !data->second.is_array()) return std::nullopt;
    return data->second.get_array();
}

/** \brief sha1(plugin|method|sorted-query): params re-serialized with keys
 *  sorted (glz::generic_sorted), so permuted query order hashes to one entry.
 *  Unparsable params hash verbatim -- the route always sends an object anyway.
 *
 *  ponytail: no response-shape version in the key -- a plugin that reshapes
 *  its payload under an unchanged query serves old-shape stale; add a version
 *  segment here when a redesign actually ships. */
inline std::string stale_key(const std::string& plugin, const std::string& method,
                             const std::string& params_json) {
    std::string canon = params_json;
    glz::generic_sorted params;
    if (!glz::read_json(params, params_json)) {
        if (auto s = glz::write_json(params)) canon = std::move(*s);
    }
    return sha1_hex(plugin + '|' + method + '|' + canon);
}

/** \brief The wrap: store look-aside -> plugin -> write-through, stale-serve
 *  on failure. Envelopes that glaze cannot re-shape pass through untouched
 *  in both directions. */
template <class Call> // std::string() -- the ABI call seam
std::string call_cached(Store& store, const std::string& id, const std::string& method,
                        const std::string& params_json, Call&& call) {
    if (!stale_optin(method)) return call();

    const std::string key = stale_key(id, method, params_json);
    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));

    // Look-aside: fresh row serves without touching the plugin. ttl_s <= 0
    // means always-expired (test hook).
    if (auto row = store.api_cache_get(id, key))
        if (row->ttl_s > 0 && row->fetched_at + row->ttl_s > now) {
            spdlog::debug("[plugin:{}] {} api_cache HIT ({} B, age={}s)", id, method,
                          row->payload.size(), now - row->fetched_at);
            return row->payload;
        } else {
            spdlog::debug("[plugin:{}] {} api_cache EXPIRED (age={}s ttl={}s), refreshing", id, method,
                          now - row->fetched_at, row->ttl_s);
        }
    else
        spdlog::debug("[plugin:{}] {} api_cache MISS", id, method);

    std::string res = call();

    glz::generic env;
    if (glz::read_json(env, res)) return res; // malformed: no shape to key on

    if (envelope_ok(env)) {
        spdlog::debug("[plugin:{}] {} fresh envelope cached", id, method);
        store.api_cache_put(id, key, params_json, res, now, kApiCacheTtl);
        return res;
    }

    spdlog::debug("[plugin:{}] {} plugin call FAILED, looking up last-good", id, method);

    if (auto row = store.api_cache_get(id, key)) {
        spdlog::debug("[plugin:{}] {} failed -> stale-serve", id, method);
        glz::generic served;
        if (!glz::read_json(served, row->payload) && served.is_object()) {
            served.get_object()["cached"].data = true; // top-level flag, via glaze
            if (auto out = glz::write_json(served)) return std::move(*out);
        }
        return row->payload; // unshapeable last-good still beats a fresh error
    }
    return res;
}
