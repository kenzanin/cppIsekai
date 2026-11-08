#include "bridge.hpp"

namespace plugin::lua {

sol::object json_to_sol(lua_State* L, const glz::generic& g) {
    sol::state_view sv(L);
    return std::visit(
        [&](auto&& v) -> sol::object {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_null_pointer_v<T>) {
                return sol::make_object(L, sol::nil);
            } else if constexpr (std::is_same_v<T, glz::generic::array_t>) {
                sol::table t = sol::table(L, sol::new_table());
                for (size_t i = 0; i < v.size(); ++i) {
                    t[i + 1] = json_to_sol(L, v[i]);
                }
                return sol::make_object(L, t);
            } else if constexpr (std::is_same_v<T, glz::generic::object_t>) {
                sol::table t = sol::table(L, sol::new_table());
                for (const auto& [k, val] : v) {
                    t[k] = json_to_sol(L, val);
                }
                return sol::make_object(L, t);
            } else {
                return sol::make_object(L, v); // double / string / bool
            }
        },
        g.data);
}

glz::generic sol_to_json(const sol::object& o, int depth) {
    using glz::generic;
    generic g;
    if (depth > 64) { // ponytail: cyclic tables would recurse forever; cap and degrade
        g.data = std::string("<too deep>");
        return g;
    }
    switch (o.get_type()) {
    case sol::type::lua_nil:
        g.data = nullptr;
        break;
    case sol::type::boolean:
        g.data = o.as<bool>();
        break;
    case sol::type::number:
        g.data = o.as<double>();
        break;
    case sol::type::string:
        g.data = o.as<std::string>();
        break;
    case sol::type::table: {
        // array iff the keys are exactly 1..n
        const sol::table t = o;
        size_t n = 0;
        bool array = true;
        for (const auto& kv : t) {
            ++n;
            array = array && kv.first.get_type() == sol::type::number &&
                    static_cast<long long>(kv.first.as<double>()) == static_cast<long long>(n);
        }
        if (n > 0 && array) {
            auto& arr = g.data.emplace<generic::array_t>();
            for (size_t i = 1; i <= n; ++i) {
                arr.push_back(sol_to_json(t[i], depth + 1));
            }
        } else {
            auto& obj = g.data.emplace<generic::object_t>();
            for (const auto& kv : t) {
                std::string key;
                if (kv.first.get_type() == sol::type::number) {
                    key = std::to_string(static_cast<long long>(kv.first.as<double>()));
                } else if (kv.first.get_type() == sol::type::string) {
                    key = kv.first.as<std::string>();
                } else {
                    continue; // non-scalar key is not representable in JSON
                }
                obj.insert({key, sol_to_json(kv.second, depth + 1)});
            }
        }
        break;
    }
    default:
        g.data = std::string("<unserializable>"); // functions/userdata: keep the envelope valid
    }
    return g;
}

} // namespace plugin::lua
