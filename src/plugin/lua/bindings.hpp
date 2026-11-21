#pragma once

/** \file bindings.hpp
 *  \brief Host-tool binding seams (source-split-clangdoc): one topic TU per
 *  Lua table under bindings/, registered by lua_runtime::install_host_tools
 *  in the historical order. HostCtx is a view -- reference members bound to
 *  the install frame; binders capture BY VALUE what their closures outlive
 *  (same discipline as the pre-split bodies).
 */

#include "plugin/lua/bridge.hpp"

namespace plugin::lua {

/** \brief View of the install_host_tools arguments, by reference. */
struct HostCtx {
    const std::string& id;
    const Config& conf;
    const glz::generic& cfg;
    bool has_cfg;
    const StateGet& sget;
    const StateSet& sset;
    const std::string& dir;
    const LiveProfile& live;
    const std::map<std::string, std::string>& params;
};

/** \brief host.fs read/read_lib + the confined require() script (the
 *  plugins_lib fallback -- keep verbatim, plugin-host-promotion D8). */
void bind_fs(lua_State* L, sol::table host, const HostCtx& ctx);

/** \brief host.log.{info,error,debug} + host.now. */
void bind_log(lua_State* L, sol::table host, const HostCtx& ctx);

/** \brief host.url_{encode,decode,abs}. */
void bind_url(lua_State* L, sol::table host);

/** \brief global io shim (read/lines; read-only ponytail). */
void bind_io(lua_State* L);

/** \brief global json.{encode,decode}. */
void bind_json(lua_State* L);

/** \brief global config.get + host.plugin.param (const declaration lookups). */
void bind_meta(lua_State* L, sol::table host, const HostCtx& ctx);

/** \brief host.storage.{get,set} (host-owned per-plugin state). */
void bind_storage(lua_State* L, sol::table host, const HostCtx& ctx);

/** \brief host.http.{fetch,fetch_body,get_json,post_json} + shared opts. */
void bind_http(lua_State* L, const HostCtx& ctx);

/** \brief host.text/codecs/re2/crypto/browser/html tables (one TU). */
void bind_rest(lua_State* L, sol::table host, const HostCtx& ctx);

} // namespace plugin::lua
