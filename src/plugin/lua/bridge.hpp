#pragma once

/** \file bridge.hpp
 *  \brief Internal lua-layer declarations (change source-split-clangdoc).
 *
 * The lua seam's JSON<->sol value bridges, the sandbox opener and the host
 * tool installer; implementations in bridge.cpp / lua_runtime.cpp. The entry
 * adapter (lua_call_entry) and its instruction budget stay in the Host core
 * because they hang off the runtime holder.
 */

#include "plugin/runtime_common.hpp"

#include <glaze/glaze.hpp>

#include <sol/sol.hpp>

namespace plugin {

struct Config; // complete type lives in plugin.hpp

namespace lua {

/** \brief glz::generic -> sol object; arrays stay 1-based, objects map keys. */
sol::object json_to_sol(lua_State* L, const glz::generic& g);

/** \brief sol value -> glz::generic; tables are arrays iff keys are 1..n.
 *  \param depth recursion guard for cyclic tables (see the <too deep> cap). */
glz::generic sol_to_json(const sol::object& o, int depth = 0);

/** \brief Open the plugin sandbox: base/string/table/math only; os, debug and
 *  package are never registered, json/io are host tables instead. */
void open_sandbox(lua_State* L);

/** \brief Install the host tool tables (fs/log/now/io/json/config/storage/
 *  http) into the plugin state. \p cfg is the parsed config.json when
 *  \p has_cfg; storage closures route through \p sget / \p sset. */
void install_host_tools(lua_State* L, const std::string& id, const Config& conf, const glz::generic& cfg, bool has_cfg,
                        const StateGet& sget, const StateSet& sset, const std::string& dir,
                        const LiveProfile& live_profile);

} // namespace plugin::lua
} // namespace plugin
