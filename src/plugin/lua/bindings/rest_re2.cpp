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

void bind_re2(lua_State* L, sol::table host, const HostCtx& /*ctx*/) {
    sol::state_view sv(L);

    sol::table re2 = sv.create_table();
    re2["find"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::string> groups;
        try {
            groups = re2_find(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < groups.size(); ++i) {
            out[i + 1] = groups[i]; // lua arrays are 1-based
        }
        return out;
    };
    re2["match"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::string> groups;
        try {
            groups = re2_match(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < groups.size(); ++i) {
            out[i + 1] = groups[i];
        }
        return out;
    };
    // P4: find_all, gmatch (closure), replace, quote, find_index
    re2["find_all"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        std::vector<std::vector<std::string>> all;
        try {
            all = re2_find_all(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < all.size(); ++i) {
            sol::table row(ts.lua_state(), sol::new_table());
            for (size_t j = 0; j < all[i].size(); ++j) {
                row[j + 1] = all[i][j]; // 1-based per existing find/match convention
            }
            out[i + 1] = sol::make_object(ts.lua_state(), row);
        }
        return out;
    };
    re2["gmatch"] = [](sol::this_state ts, const std::string& p, const std::string& t) -> sol::object {
        // Closure iterator over matches (same 1-based groups array as find_all).
        std::vector<std::vector<std::string>> all;
        try {
            all = re2_find_all(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
        auto shared = std::make_shared<std::vector<std::vector<std::string>>>(std::move(all));
        auto* idx = new size_t(0); // heap: outlives the call
        lua_State* L = ts.lua_state();
        return sol::make_object(L, [shared, idx, L]() -> std::optional<sol::table> {
            if (*idx >= shared->size()) {
                return std::nullopt; // end
            }
            ++(*idx);
            // Build a 1-based sol::table from shared->[*idx - 1]
            sol::table row(L, sol::new_table());
            const auto& groups = (*shared)[*idx - 1];
            for (size_t j = 0; j < groups.size(); ++j) {
                row[j + 1] = groups[j];
            }
            return std::optional<sol::table>(std::move(row));
        });
    };
    re2["replace"] = [](const std::string& p, const std::string& r, const std::string& t) {
        try {
            return re2_replace(p, r, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    re2["quote"] = [](const std::string& s) { return re2_quote(s); };
    re2["find_index"] = [](const std::string& p, const std::string& t) {
        try {
            return static_cast<long long>(re2_find_index(p, t)); // 0 = not found
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    // capture: group 1 (or whole match for group-less patterns), nil on no
    // match -- the find1 shim, host-owned (plugin-host-promotion D2).
    re2["capture"] = [](const std::string& p, const std::string& t) -> std::optional<std::string> {
        try {
            return re2_capture(p, t);
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(e.what());
        }
    };
    host["re2"] = re2;

    // crypto: host-executed, plugin-declared transform. Returns (result, err) pair
    // like http.fetch so plugins can distinguish bad input from a site 403.
    sol::table crypto = sv.create_table();
}

} // namespace plugin::lua
