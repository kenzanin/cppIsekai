#include "plugin/lua/bindings.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace plugin::lua {

void bind_io(lua_State* L) {
    sol::state_view sv(L);

    // ponytail: io shim is read-only via istream; widen only when a plugin
    // needs incremental reads (io.lines multi-arg / formats).
    sol::table io = sv.create_named_table("io");
    io["read"] = []() -> std::optional<std::string> {
        std::string line;
        if (std::getline(std::cin, line)) {
            return line;
        }
        return std::nullopt;
    };
    io["lines"] = [](const std::string& path, sol::this_state ts) -> sol::object {
        auto f = std::make_shared<std::ifstream>(path);
        if (!*f) {
            throw std::runtime_error("io.lines: cannot open " + path);
        }
        lua_State* L = ts;
        return sol::make_object(L, [f]() -> std::optional<std::string> {
            std::string line;
            if (std::getline(*f, line)) {
                return line;
            }
            return std::nullopt; // iterator ends; the file closes with the closure
        });
    };
}

} // namespace plugin::lua
