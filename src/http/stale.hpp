#pragma once

/** \file stale.hpp
 *  \brief Stale-serve layer (design D3): ONE wrap around the ABI call route,
 *  no per-plugin branches.
 *
 *  Success -> persist the response envelope; failure (`ok:false` / malformed)
 *  -> last-good + `"cached":true` on top level, merged through glaze -- never
 *  string-spliced. Opt-in read-only methods only. The call itself is injected
 *  (any callable), so tests fake it hermetically. */

#include "cache/cache.hpp"
#include "cache/sha1.hpp"

#include <glaze/glaze.hpp>

#include <optional>
#include <string>
#include <utility>

/** \brief Read-only methods allowed to serve stale (exact opt-in list, design
 *  D3; `pages` lands with 3.1 and is listed here from the start). */
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

/** \brief The wrap: run \p call, then persist on success / fall back to
 *  last-good with `cached:true` on failure. Envelopes that glaze cannot
 *  re-shape pass through untouched in both directions. */
template <class Call> // std::string() -- the ABI call seam
std::string call_cached(cache::Cache& cache, const std::string& id, const std::string& method,
                        const std::string& params_json, Call&& call) {
    std::string res = call();
    if (!stale_optin(method)) return res;

    glz::generic env;
    if (glz::read_json(env, res)) return res; // malformed: no shape to key on
    const std::string key = stale_key(id, method, params_json);

    if (envelope_ok(env)) {
        cache.store(key, "api", res);
        return res;
    }

    if (auto stale = cache.peek(key, "api")) {
        glz::generic served;
        if (!glz::read_json(served, *stale) && served.is_object()) {
            served.get_object()["cached"].data = true; // top-level flag, via glaze
            if (auto out = glz::write_json(served)) return std::move(*out);
        }
        return *stale; // unshapeable last-good still beats a fresh error
    }
    return res;
}
