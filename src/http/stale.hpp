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
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <glaze/glaze.hpp>

#include <ctime>
#include <optional>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

/** Constant per-method TTL for the api_cache read model; ponytail: knob later. */
inline constexpr std::int64_t kApiCacheTtl = 24 * 3600;

/** Orphan envelope: the pair's plugin id is not registered (D7) — local rows
 *  still serve via saved_detail_response/chapters_row_response before this. */
inline constexpr const char* kOrphanEnv = R"({"ok":false,"error":"plugin removed"})";

/** \brief Gate helper: disabled and removed plugins both stay quiet; the
 *  distinction is only the envelope text. */
inline bool plugin_quiet(const Store& store, const std::string& id) {
    return store.plugin_upstream(id) != Upstream::live;
}

/** \brief Per-method host defaults; manifest `cache_ttl` (plugin-side) wins
 *  per plugin+method. Reader page URLs can carry short-lived signed session
 *  tokens (comico `d=` decays in minutes -> 410 on every image), so `pages`
 *  rows age out much faster than metadata. */
inline std::int64_t cache_ttl_for(const std::string& method) {
    if (method == "pages") {
        return 30 * 60;
    }
    return kApiCacheTtl;
}

/** \brief Host default, then the plugin's manifest override when non-zero. */
inline std::int64_t effective_ttl(const plugin::Host* plugins, const std::string& id, const std::string& method) {
    const std::int64_t def = cache_ttl_for(method);
    if (plugins == nullptr) {
        return def;
    }
    const std::int64_t o = plugins->cache_ttl_seconds(id, method);
    return o > 0 ? o : def;
}

/** \brief Read-only methods allowed to serve from api_cache (exact opt-in
 *  list; all of them, not just search). Library reads are local-first: the
 *  network is only used on a cold miss (nothing cached) or when the caller
 *  explicitly refreshes. */
inline bool stale_optin(const std::string& method) {
    return method == "search" || method == "detail" || method == "chapters" || method == "pages" ||
           method == "genres" || method == "related";
}

/** \brief `ok` of an ABI envelope; anything but a literal true is a failure
 *  to serve. */
inline bool envelope_ok(const glz::generic& g) {
    const auto* obj = std::get_if<glz::generic::object_t>(&g.data);
    if (obj == nullptr) {
        return false;
    }
    const auto* const ok = obj->find("ok");
    const auto* b = ok == obj->end() ? nullptr : std::get_if<bool>(&ok->second.data);
    return (b != nullptr) && *b;
}

/** \brief `data` of an ok ABI envelope, copied; nullopt on any malformed
 *  shape. Consumers: the reader window composition and the prefetch worker. */
inline std::optional<glz::generic::array_t> envelope_data(const std::string& envelope) {
    glz::generic parsed;
    if (glz::read_json(parsed, envelope) || !envelope_ok(parsed) || !parsed.is_object()) {
        return std::nullopt;
    }
    auto* const data = parsed.get_object().find("data");
    if (data == parsed.get_object().end() || !data->second.is_array()) {
        return std::nullopt;
    }
    return data->second.get_array();
}

/** \brief sha1(plugin|method|sorted-query): params re-serialized with keys
 *  sorted (glz::generic_sorted), so permuted query order hashes to one entry.
 *  Unparsable params hash verbatim -- the route always sends an object anyway.
 *
 *  ponytail: no response-shape version in the key -- a plugin that reshapes
 *  its payload under an unchanged query serves old-shape stale; add a version
 *  segment here when a redesign actually ships. */
inline std::string stale_key(const std::string& plugin, const std::string& method, const std::string& params_json) {
    std::string canon = params_json;
    glz::generic_sorted params;
    if (!glz::read_json(params, params_json)) {
        if (auto s = glz::write_json(params)) {
            canon = std::move(*s);
        }
    }
    return sha1_hex(plugin + '|' + method + '|' + canon);
}

/** \brief The wrap: store look-aside -> plugin -> write-through, stale-serve
 *  on failure. Envelopes that glaze cannot re-shape pass through untouched
 *  in both directions. */
template <class Call> // std::string() -- the ABI call seam
std::string call_cached(Store& store, const std::string& id, const std::string& method, const std::string& params_json,
                        Call&& call, const plugin::Host* plugins = nullptr) {
    // plugin-lifecycle 1.x: a disabled plugin is read-only -- cached envelopes
    // still serve; no cache at all answers with this and the net stays quiet.
    static constexpr const char* kDisabledEnv = R"({"ok":false,"error":"plugin disabled"})";
    if (!stale_optin(method)) {
        if (plugin_quiet(store, id)) {
            return store.plugin_upstream(id) == Upstream::orphan ? kOrphanEnv : kDisabledEnv;
        }
        return call();
    }

    const std::string key = stale_key(id, method, params_json);
    const auto now = static_cast<std::int64_t>(std::time(nullptr));

    // Local-first: any cached row wins, regardless of age. The cache TTL is
    // only written for the refresh button / test hooks to interpret; page
    // opens never trigger a plugin call when a row exists. Cold miss is the
    // sole automatic network path (it cannot be avoided).
    if (auto row = store.api_cache_get(id, key)) {
        spdlog::debug("[plugin:{}] {} api_cache SERVE ({} B, age={}s ttl={}s)", id, method, row->payload.size(),
                      now - row->fetched_at, row->ttl_s);
        glz::generic served;
        if (!glz::read_json(served, row->payload) && served.is_object()) {
            // cold hit carries no flag; an expired row is marked cached so the
            // UI can show ⬇/Update is meaningful.
            const bool fresh = row->ttl_s > 0 && row->fetched_at + row->ttl_s > now;
            if (!fresh) {
                served.get_object()["cached"].data = true;
            }
            if (auto out = glz::write_json(served)) {
                return std::move(*out);
            }
        }
        return row->payload;
    }
    spdlog::debug("[plugin:{}] {} api_cache MISS", id, method);

    if (plugin_quiet(store, id)) {
        return store.plugin_upstream(id) == Upstream::orphan ? kOrphanEnv : kDisabledEnv;
    }
    std::string res = call();

    glz::generic env;
    if (glz::read_json(env, res)) {
        return res; // malformed: no shape to key on
    }

    if (envelope_ok(env)) {
        spdlog::debug("[plugin:{}] {} fresh envelope cached", id, method);
        store.api_cache_put(id, key, params_json, res, now, effective_ttl(plugins, id, method));
        return res;
    }

    spdlog::debug("[plugin:{}] {} plugin call FAILED, looking up last-good", id, method);

    if (auto row = store.api_cache_get(id, key)) {
        spdlog::debug("[plugin:{}] {} failed -> stale-serve", id, method);
        glz::generic served;
        if (!glz::read_json(served, row->payload) && served.is_object()) {
            served.get_object()["cached"].data = true; // top-level flag, via glaze
            if (auto out = glz::write_json(served)) {
                return std::move(*out);
            }
        }
        return row->payload; // unshapeable last-good still beats a fresh error
    }
    return res;
}
