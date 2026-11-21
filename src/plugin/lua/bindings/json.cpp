#include "plugin/lua/bindings.hpp"

#include <stdexcept>

namespace plugin::lua {

void bind_json(lua_State* L) {
    sol::state_view sv(L);
    sol::table json = sv.create_named_table("json");
    json["encode"] = [](const sol::object& o) { return dump_json(sol_to_json(o)); };
    json["decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        glz::generic g;
        if (auto ec = glz::read_json(g, s)) {
            throw std::runtime_error("json.decode: invalid JSON");
        }
        lua_State* lstate = ts;
        return json_to_sol(lstate, g);
    };
}

} // namespace plugin::lua
