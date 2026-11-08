#include "bridge.hpp"

#include "plugin/http_client.hpp"
#include "plugin/plugin.hpp"

#include <spdlog/spdlog.h>

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
}

void install_host_tools(lua_State* L, const std::string& id, const Config& conf, const glz::generic& cfg, bool has_cfg,
                        const StateGet& sget, const StateSet& sset, const std::string& dir,
                        const LiveProfile& live_profile) {
    sol::state_view sv(L);

    // Routed through the default logger, whose sinks include the queue sink
    // feeding the log page (wired in main).
    // fs.read: confined read-only access into this plugin's own folder (D5).
    sol::table fs_tab = sv.create_named_table("fs");
    fs_tab["read"] = [dir](const std::string& rel) -> std::optional<std::string> {
        return fs_read(dir, rel);
    };

    sol::table log = sv.create_named_table("log");
    log["info"] = [id](const std::string& msg) { spdlog::info("[plugin:{}] {}", id, msg); };
    log["error"] = [id](const std::string& msg) { spdlog::error("[plugin:{}] {}", id, msg); };

    sv.set_function("now", [] { return static_cast<long long>(std::time(nullptr)); });

    // ponytail: io shim is read-only via istream; widen only when a plugin
    // needs incremental reads (io.lines multi-arg / formats).
    sol::table io = sv.create_named_table("io");
    io["read"] = [] -> std::optional<std::string> {
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
        return sol::make_object(L, [f] -> std::optional<std::string> {
            std::string line;
            if (std::getline(*f, line)) {
                return line;
            }
            return std::nullopt; // iterator ends; the file closes with the closure
        });
    };

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

    // config.get: values from the plugin's own config.json, parsed once at load.
    // Absent file or key -> nil; the sandbox itself stays file-read-free.
    sol::table config = sv.create_named_table("config");
    config["get"] = [&cfg, has_cfg](const std::string& key, sol::this_state ts) -> sol::object {
        const auto* obj = std::get_if<glz::generic::object_t>(&cfg.data);
        if (!has_cfg || obj == nullptr) {
            return sol::make_object(ts, sol::lua_nil);
        }
        if (auto it = obj->find(key); it != obj->end()) {
            return json_to_sol(ts, it->second);
        }
        return sol::make_object(ts, sol::lua_nil);
    };

    // storage.get/set: host-owned per-plugin state (tokens), db-backed, survives restarts.
    sol::table storage = sv.create_named_table("storage");
    storage["get"] = [id, sget](const std::string& key, sol::this_state ts) -> sol::object {
        if (auto v = sget(id, key)) {
            return sol::make_object(ts, *v);
        }
        return sol::make_object(ts, sol::lua_nil);
    };
    storage["set"] = [id, sset](const std::string& key, const std::string& val) { sset(id, key, val); };

    // Blocking fetch on the calling (plugin-pool) worker; curl's own timeout
    // guards the network half, the instruction hook the CPU half.
    sol::table http = sv.create_named_table("http");
    http["fetch"] = [id, timeout_ms = conf.fetch_timeout_ms, live = &live_profile,
                     proxy = conf.proxy](
                        const std::string& url, const sol::optional<sol::table>& opts,
                        sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] fetch {} failed: {}", id, url, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };

        long ms = timeout_ms;
        std::string live_default;
        if (auto p = live->load()) {
            live_default = *p;
        }
        std::string profile_name = live_default;
        std::string method = "GET";
        std::string body;
        std::string accept_header; // optional per-call Accept override
        std::string ua_header;     // optional per-call User-Agent override
        std::vector<std::pair<std::string, std::string>> extra_headers;
        if (opts) {
            if (sol::object v = (*opts)["timeout_ms"]; v.get_type() == sol::type::number) {
                ms = static_cast<long>(v.as<double>());
            }
            if (sol::object v = (*opts)["profile"]; v.get_type() == sol::type::string) {
                profile_name = v.as<std::string>();
            }
            if (sol::object v = (*opts)["method"]; v.get_type() == sol::type::string) {
                method = v.as<std::string>();
            }
            if (sol::object v = (*opts)["body"]; v.get_type() == sol::type::string) {
                body = v.as<std::string>();
            }
            if (sol::object v = (*opts)["accept"]; v.get_type() == sol::type::string) {
                accept_header = v.as<std::string>();
            }
            if (sol::object v = (*opts)["user_agent"]; v.get_type() == sol::type::string) {
                ua_header = v.as<std::string>();
            }
            if (sol::object v = (*opts)["headers"]; v.get_type() == sol::type::table) {
                v.as<sol::table>().for_each([&](sol::object k, sol::object val) {
                    if (k.get_type() == sol::type::string && val.get_type() == sol::type::string) {
                        extra_headers.emplace_back(k.as<std::string>(), val.as<std::string>());
                    }
                });
            }
        }

        profile_name = checked_profile(std::move(profile_name), live_default);
        // The shared sync path (http_client): one option block, retry, impersonation.
        const FetchResult fr =
            fetch_sync(url, ms, profile_name, method, body, accept_header, ua_header, extra_headers, proxy);
        if (!fr.error.empty()) {
            return fail(fr.error);
        }
        const long code = fr.status;

        sol::table out = sol::table(ts.lua_state(), sol::new_table());
        out["status"] = static_cast<long long>(code);
        out["body"] = fr.body;
        sol::table hh = sol::table(ts.lua_state(), sol::new_table());
        for (const auto& [k, v] : fr.headers) {
            hh[k] = v;
        }
        out["headers"] = sol::make_object(ts, hh);
        return {sol::make_object(ts, out), std::nullopt};
    };
}

} // namespace plugin::lua
