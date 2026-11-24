#include "plugin/lua/bindings.hpp"

#include "plugin/http_client.hpp"

#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace plugin::lua {

namespace {

// Shared request-opts surface for host.http.fetch/fetch_body (hot-path-perf P2):
// one parser, two consumers -- the option set can no longer drift between them.
struct HttpOpts {
    long ms{};                // effective timeout; conf default when unset
    std::string live_default; // live-profile snapshot at call time
    std::string profile;      // == live_default until opts/profile overrides
    std::string method = "GET";
    std::string body;
    std::string accept;
    std::string user_agent;
    bool cookie = false; // per-plugin cookie jar
    std::vector<std::pair<std::string, std::string>> headers;
};

HttpOpts parse_http_opts(const sol::optional<sol::table>& opts, long timeout_ms, const plugin::LiveProfile& live) {
    HttpOpts o;
    o.ms = timeout_ms;
    if (auto p = live.load()) {
        o.live_default = *p;
    }
    o.profile = o.live_default;
    if (opts) {
        if (sol::object v = (*opts)["timeout_ms"]; v.get_type() == sol::type::number) {
            o.ms = static_cast<long>(v.as<double>());
        }
        if (sol::object v = (*opts)["profile"]; v.get_type() == sol::type::string) {
            o.profile = v.as<std::string>();
        }
        if (sol::object v = (*opts)["method"]; v.get_type() == sol::type::string) {
            o.method = v.as<std::string>();
        }
        if (sol::object v = (*opts)["body"]; v.get_type() == sol::type::string) {
            o.body = v.as<std::string>();
        }
        if (sol::object v = (*opts)["accept"]; v.get_type() == sol::type::string) {
            o.accept = v.as<std::string>();
        }
        if (sol::object v = (*opts)["user_agent"]; v.get_type() == sol::type::string) {
            o.user_agent = v.as<std::string>();
        }
        if (sol::object v = (*opts)["cookie"]; v.get_type() == sol::type::boolean) {
            o.cookie = v.as<bool>();
        }
        if (sol::object v = (*opts)["headers"]; v.get_type() == sol::type::table) {
            v.as<sol::table>().for_each([&](const sol::object& k, const sol::object& val) {
                if (k.get_type() == sol::type::string && val.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), val.as<std::string>());
                }
            });
        }
    }
    return o;
}

// P5 → plugin-host-promotion D3: get_json takes the full fetch opts
// envelope (timeout_ms/profile/headers/cookie/...); a table with none of
// the known keys is the legacy flat headers map, so old call sites keep
// working. Returns (value, err). post_json keeps its (url, body, headers)
// contract and single return.
std::pair<sol::object, std::optional<std::string>> json_fetch(const std::string& id, const std::string& proxy,
                                                              const std::string& url, HttpOpts o,
                                                              sol::this_state ts) {
    const std::string profile = checked_profile(std::move(o.profile), o.live_default);
    const std::string cookie_jar = o.cookie ? cookie_jar_path(id) : std::string{};
    const FetchResult fr =
        fetch_sync(url, o.ms, profile, o.method, o.body, o.accept, o.user_agent, o.headers, proxy, cookie_jar);
    if (!fr.error.empty()) {
        spdlog::warn("[plugin:{}] json_fetch {} failed: {}", id, url, fr.error);
        return {sol::make_object(ts, sol::lua_nil), std::optional(fr.error)};
    }
    if (fr.status != 200) {
        return {sol::make_object(ts, sol::lua_nil), std::optional("upstream status " + std::to_string(fr.status))};
    }
    glz::generic g;
    if (auto ec = glz::read_json(g, fr.body)) {
        return {sol::make_object(ts, sol::lua_nil), std::optional(std::string("invalid JSON body"))};
    }
    return {json_to_sol(ts, g), std::nullopt};
}

} // namespace

void bind_http(lua_State* L, const HostCtx& ctx) {
    sol::state_view sv(L);
    const std::string& id = ctx.id;
    const auto timeout_ms = ctx.conf.fetch_timeout_ms;
    const std::string proxy = ctx.conf.proxy;

    // Blocking fetch on the calling (plugin-pool) worker; curl's own timeout
    // guards the network half, the instruction hook the CPU half.
    sol::table http = sv.create_table();
    http["fetch"] = [id, timeout_ms, live = &ctx.live,
                     proxy](const std::string& url, const sol::optional<sol::table>& opts,
                            sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] fetch {} failed: {}", id, url, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };

        const HttpOpts o = parse_http_opts(opts, timeout_ms, *live);

        const std::string profile = checked_profile(std::move(o.profile), o.live_default);
        const std::string cookie_jar = o.cookie ? cookie_jar_path(id) : std::string{};
        // The shared sync path (http_client): one option block, retry, impersonation.
        const FetchResult fr =
            fetch_sync(url, o.ms, profile, o.method, o.body, o.accept, o.user_agent, o.headers, proxy, cookie_jar);
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

    // host.http.fetch_body: (body, err); body nil unless status == 200
    // (plugin-host-promotion D4 -- the (body,err) pair every http_req shim
    // hand-rolled). Second value is nil on success.
    http["fetch_body"] = [id, timeout_ms, live = &ctx.live,
                          proxy](const std::string& url, const sol::optional<sol::table>& opts,
                                 sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const HttpOpts o = parse_http_opts(opts, timeout_ms, *live);

        const std::string profile = checked_profile(std::move(o.profile), o.live_default);
        const std::string cookie_jar = o.cookie ? cookie_jar_path(id) : std::string{};
        const FetchResult fr =
            fetch_sync(url, o.ms, profile, o.method, o.body, o.accept, o.user_agent, o.headers, proxy, cookie_jar);
        if (!fr.error.empty()) {
            spdlog::warn("[plugin:{}] fetch_body {} failed: {}", id, url, fr.error);
            return {sol::make_object(ts, sol::lua_nil), std::optional(fr.error)};
        }
        if (fr.status != 200) {
            return {sol::make_object(ts, sol::lua_nil), std::optional("upstream status " + std::to_string(fr.status))};
        }
        return {sol::make_object(ts, fr.body), std::nullopt};
    };

    http["get_json"] = [id, timeout_ms, live = &ctx.live,
                        proxy](const std::string& url, const sol::optional<sol::table>& opts,
                               sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        HttpOpts o;
        o.ms = timeout_ms;
        if (auto p = live->load()) {
            o.live_default = *p;
        }
        o.profile = o.live_default;
        bool has_known = false;
        if (opts) {
            opts->for_each([&](const sol::object& k, const sol::object&) {
                if (k.get_type() == sol::type::string) {
                    const std::string key = k.as<std::string>();
                    if (key == "timeout_ms" || key == "profile" || key == "method" || key == "body" ||
                        key == "accept" || key == "user_agent" || key == "cookie" || key == "headers") {
                        has_known = true;
                    }
                }
            });
        }
        if (opts && has_known) {
            o = parse_http_opts(opts, timeout_ms, *live);
        } else if (opts) { // legacy: flat string->string map == headers
            opts->for_each([&](const sol::object& k, const sol::object& v) {
                if (k.get_type() == sol::type::string && v.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), v.as<std::string>());
                }
            });
        }
        return json_fetch(id, proxy, url, std::move(o), ts);
    };
    http["post_json"] = [id, timeout_ms, live = &ctx.live,
                         proxy](const std::string& url, const std::string& body,
                                const sol::optional<sol::table>& headers, sol::this_state ts) -> sol::object {
        HttpOpts o;
        o.ms = timeout_ms;
        o.method = "POST";
        o.body = body;
        if (auto p = live->load()) {
            o.live_default = *p;
        }
        o.profile = o.live_default;
        if (headers) {
            headers->for_each([&](const sol::object& k, const sol::object& v) {
                if (k.get_type() == sol::type::string && v.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), v.as<std::string>());
                }
            });
        }
        return json_fetch(id, proxy, url, std::move(o), ts).first;
    };
    sv["host"]["http"] = http;
}

} // namespace plugin::lua
