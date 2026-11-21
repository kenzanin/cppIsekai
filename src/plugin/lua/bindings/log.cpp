#include "plugin/lua/bindings.hpp"

#include <spdlog/spdlog.h>

#include <ctime>

namespace plugin::lua {

void bind_log(lua_State* L, sol::table host, const HostCtx& ctx) {
    sol::state_view sv(L);
    sol::table log = sv.create_table();
    log["info"] = [id = ctx.id](const std::string& msg) { spdlog::info("[plugin:{}] {}", id, msg); };
    log["error"] = [id = ctx.id](const std::string& msg) { spdlog::error("[plugin:{}] {}", id, msg); };
    log["debug"] = [id = ctx.id](const std::string& msg) { spdlog::debug("[plugin:{}] {}", id, msg); };
    host["log"] = log;

    host["now"] = sol::as_function([] { return static_cast<long long>(std::time(nullptr)); });
}

} // namespace plugin::lua
