#pragma once

/** \file runtime_common.hpp
 *  \brief Types and tiny helpers shared by every plugin-runtime seam.
 *
 * Hoisted from wasm_rt.hpp when the runtimes moved to per-runtime folders
 * (change source-split-clangdoc): the lua seam fills \ref plugin::RawReply,
 * wasm and js consume it, and all three take the same host-service bundle.
 * The inline helpers here are the shared JSON utilities the cores and seams
 * use on the envelope/reply path.
 */

#include <glaze/glaze.hpp>

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace plugin {

/** Host-service accessors: (plugin_id, key) -> value. Routed to the db worker
 *  when one is attached; inline otherwise (tests). */
using StateGet = std::function<std::optional<std::string>(const std::string& plugin_id, const std::string& key)>;

/** Host-service mutator: (plugin_id, key, value), persistent across restarts. */
using StateSet = std::function<void(const std::string& plugin_id, const std::string& key, const std::string& value)>;

/** \brief Synchronous fetch front used by every runtime: url + JSON-encoded
 *  opts in, the lua-shaped result ({"status","body","headers"}) as JSON out;
 *  nullopt = fetch failure (the wasm import reports -1, js gets null). */
using HttpFetch = std::function<std::optional<std::string>(const std::string& url, const std::string& opts_json)>;

/** Host services the guest reaches through env imports or globals, bound per
 *  plugin at load. nullopt results are the import's -1 (domain failure). */
struct Services {
    StateGet state_get;   /**< per-plugin persistent read */
    StateSet state_set;   /**< per-plugin persistent write */
    HttpFetch http_fetch; /**< blocking fetch through the shared curl path */
};

/** \brief Raw reply handed back to the shared core: the plugin's return value
 *  marshaled as-is, shape-checked later. When the entry never produced one,
 *  \p invoked is false and \p error carries the host-side diagnostic
 *  (already logged). */
struct RawReply {
    bool invoked = false; /**< whether the entry produced a reply at all */
    std::string error;    /**< host-side diagnostic when !invoked */
    glz::generic reply;   /**< the reply value; shape-checked by the core gate */
};

/** \brief Serialize a glz::generic; "null" on write failure so envelopes stay
 *  valid JSON. */
inline std::string dump_json(const glz::generic& g) {
    if (auto s = glz::write_json(g)) {
        return std::move(*s);
    }
    return "null";
}

/** \brief Object member view: nullptr when there is no object or the key is
 *  absent. */
inline const glz::generic* member(const glz::generic::object_t* obj, const char* key) {
    if (obj == nullptr) {
        return nullptr;
    }
    const auto* const it = obj->find(std::string(key));
    return it != obj->end() ? &it->second : nullptr;
}

} // namespace plugin
