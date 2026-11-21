#include "plugin/lua/bindings.hpp"

namespace plugin::lua {

void bind_storage(lua_State* L, sol::table host, const HostCtx& ctx) {
    sol::state_view sv(L);

    // storage.get/set: host-owned per-plugin state (tokens), db-backed, survives restarts.
    sol::table storage = sv.create_table();
    storage["get"] = [id = ctx.id, sget = ctx.sget](const std::string& key, sol::this_state ts) -> sol::object {
        if (auto v = sget(id, key)) {
            return sol::make_object(ts, *v);
        }
        return sol::make_object(ts, sol::lua_nil);
    };
    storage["set"] = [id = ctx.id, sset = ctx.sset](const std::string& key, const std::string& val) {
        sset(id, key, val);
    };
    host["storage"] = storage;
}

} // namespace plugin::lua
