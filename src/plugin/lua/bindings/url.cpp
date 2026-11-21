#include "plugin/lua/bindings.hpp"

#include "plugin/url_codec.hpp"

namespace plugin::lua {

void bind_url(lua_State* L, sol::table host) {
    sol::state_view sv(L);
    // host.url_encode/url_decode: shared query-component codec (src/plugin/url_codec.hpp);
    // decode leaves '+' verbatim (queryEncode semantics, goIsekai parity).
    host["url_encode"] = [](const std::string& s) { return plugin::url_encode(s); };
    host["url_decode"] = [](const std::string& s) { return plugin::url_decode(s); };
    // url_abs: madaradex semantics, canonical since plugin-host-promotion D6.
    host["url_abs"] = [](const std::string& base, const std::string& ref) { return plugin::url_abs(base, ref); };
}

} // namespace plugin::lua
