#include "plugin/lua/bindings.hpp"

#include "browser/chrome.hpp"
#include "plugin/host_codecs.hpp"
#include "plugin/host_crypto.hpp"
#include "plugin/host_text.hpp"
#include "plugin/host_xpath.hpp"
#include "plugin/htmltext.hpp"
#include "plugin/url_codec.hpp"

#include <spdlog/spdlog.h>

namespace plugin::lua {

void bind_text_codecs(lua_State*, sol::table, const HostCtx&);
void bind_re2(lua_State*, sol::table, const HostCtx&);
void bind_crypto_html(lua_State*, sol::table, const HostCtx&);

void bind_rest(lua_State* L, const sol::table& host, const HostCtx& ctx) {
    bind_text_codecs(L, host, ctx);
    bind_re2(L, host, ctx);
    bind_crypto_html(L, host, ctx);
}

} // namespace plugin::lua
