#include "bridge.hpp"
#include "bindings.hpp"
#include "browser/chrome.hpp"
#include "plugin/url_codec.hpp"

#include "plugin/host_text.hpp"
#include "plugin/htmltext.hpp" // host.re2/html core (task 1.3)
#include "plugin/http_client.hpp"
#include "plugin/host_codecs.hpp"
#include "plugin/host_crypto.hpp"
#include "plugin/host_xpath.hpp"
#include "plugin/plugin.hpp"
#include "plugin/vrf.hpp"

#include <map>
#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace plugin::lua {

void open_sandbox(lua_State* L) {
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);
    lua_pop(L, 1);
}

// Host tools for the Lua runtime. Build with clang only (xmake set_toolchains):
// GCC folds every same-signature lambda to ONE type name, and sol2 3.5.0 keys its
// functor metatables on that name (upstream #1743) -- at lua_close it then runs the
// wrong destructor and segfaults. clang names include the source location, so the
// keys stay distinct. See knowledge/findings/sol2-lua-functor-teardown-corruption.
void install_host_tools(lua_State* L, const std::string& id, const Config& conf, const glz::generic& cfg, bool has_cfg,
                        const StateGet& sget, const StateSet& sset, const std::string& dir,
                        const LiveProfile& live_profile, const std::map<std::string, std::string>& params) {
    sol::state_view sv(L);
    sol::table host = sv.create_named_table("host");
    const HostCtx ctx{id, conf, cfg, has_cfg, sget, sset, dir, live_profile, params};

    // Historical table names/keys are unchanged; bodies now live one topic per
    // TU under bindings/. Registration order is preserved.
    bind_fs(L, host, ctx);
    bind_log(L, host, ctx);
    bind_url(L, host);
    bind_io(L);
    bind_json(L);
    bind_meta(L, host, ctx);
    bind_storage(L, host, ctx);
    bind_http(L, ctx);
    bind_rest(L, host, ctx);
}

} // namespace plugin::lua
