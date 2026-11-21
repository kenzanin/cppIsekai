#include "plugin/lua/bindings.hpp"

namespace plugin::lua {

void bind_meta(lua_State* L, sol::table host, const HostCtx& ctx) {
    sol::state_view sv(L);

    // config.get: values from the plugin's own config.json, parsed once at load.
    // Absent file or key -> nil; the sandbox itself stays file-read-free.
    // Captured BY VALUE: rt.cfg dies with scan_dir's frame, so a reference capture
    // dangles for every config.get call made after plugin load.
    sol::table config = sv.create_named_table("config");
    config["get"] = [cfg = ctx.cfg, has_cfg = ctx.has_cfg](const std::string& key, sol::this_state ts) -> sol::object {
        const auto* obj = std::get_if<glz::generic::object_t>(&cfg.data);
        if (!has_cfg || obj == nullptr) {
            return sol::make_object(ts, sol::lua_nil);
        }
        if (auto it = obj->find(key); it != obj->end()) {
            return json_to_sol(ts, it->second);
        }
        return sol::make_object(ts, sol::lua_nil);
    };

    // host.plugin.param: read manifest params; nil when absent. By value for the
    // same lifetime reason as config.get.
    sol::table plugin_tbl = sv.create_table();
    plugin_tbl["param"] = [params = ctx.params](const std::string& key, sol::this_state ts) -> sol::object {
        const auto it = params.find(key);
        if (it != params.end()) {
            return sol::make_object(ts, it->second);
        }
        return sol::make_object(ts, sol::lua_nil);
    };
    host["plugin"] = plugin_tbl;
}

} // namespace plugin::lua
