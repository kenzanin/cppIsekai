#include "bridge.hpp"
#include "browser/chrome.hpp"
#include "plugin/url_codec.hpp"

#include "plugin/host_text.hpp"
#include "plugin/htmltext.hpp" // host.re2/html core (task 1.3)
#include "plugin/http_client.hpp"
#include "plugin/host_codecs.hpp"
#include "plugin/host_crypto.hpp"
#include "plugin/host_xpath.hpp"
#include "plugin/plugin.hpp"
#include "plugin/vrf.hpp"

#include <map>
#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdlib>
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
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1);
    lua_pop(L, 1);
}

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
            v.as<sol::table>().for_each([&](sol::object k, sol::object val) {
                if (k.get_type() == sol::type::string && val.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), val.as<std::string>());
                }
            });
        }
    }
    return o;
}

} // namespace

// Host tools for the Lua runtime. Build with clang only (xmake set_toolchains):
// GCC folds every same-signature lambda to ONE type name, and sol2 3.5.0 keys its
// functor metatables on that name (upstream #1743) -- at lua_close it then runs the
// wrong destructor and segfaults. clang names include the source location, so the
// keys stay distinct. See knowledge/findings/sol2-lua-functor-teardown-corruption.
void install_host_tools(lua_State* L, const std::string& id, const Config& conf, const glz::generic& cfg, bool has_cfg,
                        const StateGet& sget, const StateSet& sset, const std::string& dir,
                        const LiveProfile& live_profile, const std::map<std::string, std::string>& params) {
    sol::state_view sv(L);

    sol::table fs_tab = sv.create_named_table("host");
    sol::table fs = sv.create_table();
    fs["read"] = [dir](const std::string& rel) -> std::optional<std::string> { return fs_read(dir, rel); };
    // Shared read-only lib root: <data>/plugins_lib -> plugins_dev/lib (two
    // levels above the per-plugin dir). require()'s fallback, never written by
    // plugins. plugin-host-promotion D8.
    fs["read_lib"] = [dir](const std::string& rel) -> std::optional<std::string> {
        const std::filesystem::path root = std::filesystem::path(dir).parent_path().parent_path() / "plugins_lib";
        return fs_read(root, rel);
    };
    fs_tab["fs"] = fs;

    // Confined require() as a pure-Lua helper on top of host.fs.read:
    // no C++ stack/object lifetime to get wrong across state teardown.
    // Resolves "a.b" -> <dir>/a/b.lua (or a/b/init.lua); per-state cache in a
    // hidden global; path segments are checked for traversal in Lua.
    sv.script(R"lua(
local fs_read = host.fs.read
local fs_read_lib = host.fs.read_lib
local cache = {}
local function fail(msg) error("require: " .. msg, 0) end
function require(name)
  if type(name) ~= "string" or name == "" then fail("empty module name") end
  local hit = cache[name]
  if hit ~= nil then return hit end
  local rel = (name:gsub("%.", "/"))
  if rel:sub(1, 1) == "/" then fail("bad module name '" .. name .. "'") end
  for seg in rel:gmatch("[^/]+") do
    if seg == "." or seg == ".." then fail("bad module name '" .. name .. "'") end
  end
  local src, chunkname = fs_read(rel .. ".lua"), "@" .. rel .. ".lua"
  if src == nil then
    src, chunkname = fs_read(rel .. "/init.lua"), "@" .. rel .. "/init.lua"
  end
  if src == nil and fs_read_lib then -- shared lib fallback (read-only)
    src, chunkname = fs_read_lib(rel .. ".lua"), "@lib/" .. rel .. ".lua"
    if src == nil then
      src, chunkname = fs_read_lib(rel .. "/init.lua"), "@lib/" .. rel .. "/init.lua"
    end
  end
  if src == nil then fail("module '" .. name .. "' not found") end
  local chunk, err = load(src, chunkname)
  if chunk == nil then fail(tostring(err)) end
  local ok, result = pcall(chunk)
  if not ok then fail(tostring(result)) end
  if result == nil then result = true end
  cache[name] = result
  return result
end
)lua");

    sol::table log = sv.create_table();
    log["info"] = [id](const std::string& msg) { spdlog::info("[plugin:{}] {}", id, msg); };
    log["error"] = [id](const std::string& msg) { spdlog::error("[plugin:{}] {}", id, msg); };
    log["debug"] = [id](const std::string& msg) { spdlog::debug("[plugin:{}] {}", id, msg); };
    fs_tab["log"] = log;

    fs_tab["now"] = sol::as_function([] { return static_cast<long long>(std::time(nullptr)); });

    // host.url_encode/url_decode: shared query-component codec (src/plugin/url_codec.hpp);
    // decode leaves '+' verbatim (queryEncode semantics, goIsekai parity).
    fs_tab["url_encode"] = [](const std::string& s) { return plugin::url_encode(s); };
    fs_tab["url_decode"] = [](const std::string& s) { return plugin::url_decode(s); };
    // url_abs: madaradex semantics, canonical since plugin-host-promotion D6.
    fs_tab["url_abs"] = [](const std::string& base, const std::string& ref) { return plugin::url_abs(base, ref); };

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
    // Captured BY VALUE: rt.cfg dies with scan_dir's frame, so a reference capture
    // dangles for every config.get call made after plugin load.
    sol::table config = sv.create_named_table("config");
    config["get"] = [cfg, has_cfg](const std::string& key, sol::this_state ts) -> sol::object {
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
    sol::table storage = sv.create_table();
    storage["get"] = [id, sget](const std::string& key, sol::this_state ts) -> sol::object {
        if (auto v = sget(id, key)) {
            return sol::make_object(ts, *v);
        }
        return sol::make_object(ts, sol::lua_nil);
    };
    storage["set"] = [id, sset](const std::string& key, const std::string& val) { sset(id, key, val); };
    fs_tab["storage"] = storage;

    // host.plugin.param: read manifest params; nil when absent. By value for the
    // same lifetime reason as config.get.
    sol::table plugin_tbl = sv.create_table();
    plugin_tbl["param"] = [params](const std::string& key, sol::this_state ts) -> sol::object {
        const auto it = params.find(key);
        if (it != params.end()) {
            return sol::make_object(ts, it->second);
        }
        return sol::make_object(ts, sol::lua_nil);
    };
    fs_tab["plugin"] = plugin_tbl;

    // Blocking fetch on the calling (plugin-pool) worker; curl's own timeout
    // guards the network half, the instruction hook the CPU half.
    sol::table http = sv.create_table();
    http["fetch"] = [id, timeout_ms = conf.fetch_timeout_ms, live = &live_profile,
                     proxy = conf.proxy](const std::string& url, const sol::optional<sol::table>& opts,
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
    http["fetch_body"] = [id, timeout_ms = conf.fetch_timeout_ms, live = &live_profile,
                          proxy = conf.proxy](const std::string& url, const sol::optional<sol::table>& opts,
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

    // P5 → plugin-host-promotion D3: get_json takes the full fetch opts
    // envelope (timeout_ms/profile/headers/cookie/...); a table with none of
    // the known keys is the legacy flat headers map, so old call sites keep
    // working. Returns (value, err). post_json keeps its (url, body, headers)
    // contract and single return.
    const auto json_fetch = [id, proxy = conf.proxy](const std::string& url, HttpOpts o,
                                                 sol::this_state ts)
        -> std::pair<sol::object, std::optional<std::string>> {
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
    };
    http["get_json"] = [json_fetch, timeout_ms = conf.fetch_timeout_ms, live = &live_profile](
                           const std::string& url, const sol::optional<sol::table>& opts,
                           sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        HttpOpts o;
        o.ms = timeout_ms;
        if (auto p = live->load()) {
            o.live_default = *p;
        }
        o.profile = o.live_default;
        bool has_known = false;
        if (opts) {
            opts->for_each([&](sol::object k, sol::object) {
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
            opts->for_each([&](sol::object k, sol::object v) {
                if (k.get_type() == sol::type::string && v.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), v.as<std::string>());
                }
            });
        }
        return json_fetch(url, std::move(o), ts);
    };
    http["post_json"] = [json_fetch, timeout_ms = conf.fetch_timeout_ms, live = &live_profile](
                            const std::string& url, const std::string& body,
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
            headers->for_each([&](sol::object k, sol::object v) {
                if (k.get_type() == sol::type::string && v.get_type() == sol::type::string) {
                    o.headers.emplace_back(k.as<std::string>(), v.as<std::string>());
                }
            });
        }
        return json_fetch(url, std::move(o), ts).first;
    };
    fs_tab["http"] = http;

    // host.text: status normalization, chapter-number extraction, date→ISO
    // (P2, host_text.hpp — one core, three thin marshals; design D4).
    sol::table text_tbl = sv.create_table();
    text_tbl["normalize_status"] = [](const std::string& s) { return plugin::host_text::normalize_status(s); };
    text_tbl["chapter_num"] = [](const std::string& s) -> std::optional<double> {
        const auto v = plugin::host_text::chapter_num(s);
        return v ? std::optional<double>(*v) : std::nullopt;
    };
    text_tbl["date_to_iso"] = [](const std::string& s) -> std::optional<std::string> {
        return plugin::host_text::date_to_iso(s);
    };
    text_tbl["date_epoch"] = [](const std::string& s) -> std::optional<long long> {
        return plugin::host_text::date_epoch(s);
    };
    text_tbl["json_blob"] = [](const std::string& s, const std::string& marker, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_text::json_blob(s, marker);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    text_tbl["trim"] = [](const std::string& s) { return plugin::host_text::trim(s); };
    text_tbl["unescape"] = [](const std::string& s) { return plugin::host_text::unescape(s); };
    text_tbl["strip_html"] = [](const std::string& s) { return plugin::host_text::strip_html(s); };
    fs_tab["text"] = text_tbl;

    // host.codecs: base64/hex encodings (P6, host_codecs.hpp one core, thin
    // marshals); decode nil on invalid, the house optional contract.
    sol::table codec_tbl = sv.create_table();
    codec_tbl["base64_encode"] = [](const std::string& s) { return plugin::host_codec::base64_encode(s); };
    codec_tbl["base64_decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_codec::base64_decode(s);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    codec_tbl["hex_encode"] = [](const std::string& s) { return plugin::host_codec::hex_encode(s); };
    codec_tbl["hex_decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        const auto v = plugin::host_codec::hex_decode(s);
        return v ? sol::make_object(ts, *v) : sol::make_object(ts, sol::lua_nil);
    };
    fs_tab["codecs"] = codec_tbl;

    // Host-side heavy primitives (design D4 one core, thin marshals): RE2
    // (linear-time, no ReDoS) and lexbor DOM selectors. Bad patterns throw
    // (loud); no-match / malformed HTML return empty lists.
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
    fs_tab["re2"] = re2;

    // crypto: host-executed, plugin-declared transform. Returns (result, err) pair
    // like http.fetch so plugins can distinguish bad input from a site 403.
    sol::table crypto = sv.create_table();
    crypto["vrf_sign"] = [id](const std::string& api_path, const sol::optional<sol::table>& params,
                              const sol::optional<sol::table>& stages,
                              sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] vrf_sign {} failed: {}", id, api_path, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };
        if (api_path.empty())
            return fail("vrf_sign: api_path is empty");

        std::map<std::string, std::string> prm;
        if (params) {
            for (auto& [k, v] : *params) {
                if (!k.is<std::string>())
                    return fail("vrf_sign: params keys must be strings");
                if (v.is<std::string>()) {
                    prm[k.as<std::string>()] = v.as<std::string>();
                } else if (v.is<bool>()) {
                    prm[k.as<std::string>()] = v.as<bool>() ? "true" : "false";
                } else if (v.is<int>() || v.is<double>() || v.is<float>()) {
                    prm[k.as<std::string>()] = std::to_string(v.as<double>());
                } else {
                    return fail("vrf_sign: params[\"" + k.as<std::string>() + "\"] must be scalar");
                }
            }
        }

        std::vector<VrfStage> vstages;
        if (!stages || stages->size() == 0)
            return fail("vrf_sign: stages must be a non-empty array");
        for (size_t i = 0; i < stages->size(); ++i) {
            const size_t idx = i + 1;
            const sol::optional<sol::table> st = (*stages)[idx];
            if (!st)
                return fail("vrf_sign: stage " + std::to_string(idx) + ": missing");
            const sol::optional<double> iv = (*st)["iv"];
            const sol::optional<std::string> key = (*st)["key"];
            const sol::optional<std::string> tbl = (*st)["tbl"];
            if (!iv || !key || !tbl)
                return fail("vrf_sign: stage " + std::to_string(idx) + ": needs iv, key, tbl");
            const double fid = *iv;
            const int ivv = static_cast<int>(fid);
            if (fid != static_cast<double>(ivv) || ivv < 0 || ivv > 255) {
                return fail("vrf_sign: stage " + std::to_string(idx) + ": iv must be integer 0-255");
            }
            vstages.push_back({static_cast<uint8_t>(ivv), *key, *tbl});
        }

        std::string err;
        auto out = vrf_sign(api_path, prm, vstages, err);
        if (!out) {
            spdlog::warn("[plugin:{}] vrf_sign {} failed: {}", id, api_path, err);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(err)));
        }
        return std::make_pair(sol::make_object(ts, *out), std::optional<std::string>{});
    };
    // host.crypto P7 (host_crypto.hpp): hex in/out; nil on tag/hex/key failure.
    crypto["sha256_hex"] = [](const std::string& s) { return plugin::host_crypto::sha256_hex(s); };
    crypto["md5_hex"] = [](const std::string& s) { return plugin::host_crypto::md5_hex(s); };
    crypto["hmac_sha256_hex"] = [](const std::string& data, const std::string& key) {
        return plugin::host_crypto::hmac_sha256_hex(data, key);
    };
    crypto["aes_gcm_decrypt"] = [](const std::string& iv, const std::string& ct, const std::string& tag,
                                   const std::string& key, sol::this_state ts) -> sol::object {
        const auto pt = plugin::host_crypto::aes_gcm_decrypt(iv, ct, tag, key);
        return pt ? sol::make_object(ts, *pt) : sol::make_object(ts, sol::lua_nil);
    };
    fs_tab["crypto"] = crypto;

    // browser (change browser-host D5): one passthrough, host.browser.call(tool, args)
    // -> (result, err) like http.fetch. args = Lua table (encoded here) or raw JSON
    // string; result is the tool text, err carries transport/RPC/isError diagnostics.
    sol::table browser_tab = sv.create_table();
    browser_tab["call"] = [id](const std::string& tool, const sol::object& args, sol::this_state ts)
                              -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] browser.{} failed: {}", id, tool, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };
        std::string args_json;
        if (args.is<std::string>()) {
            args_json = args.as<std::string>();
        } else if (args.is<sol::table>()) {
            args_json = dump_json(sol_to_json(args));
        } else if (!args.valid() || args == sol::lua_nil) {
            args_json = "{}";
        } else {
            return fail("browser: args must be a table or json string");
        }
        std::string err;
        std::string out = ::browser::call(tool, args_json, &err);
        if (out.empty() && !err.empty()) {
            return fail(std::move(err));
        }
        return std::make_pair(sol::make_object(ts, std::move(out)), std::optional<std::string>{});
    };
    fs_tab["browser"] = browser_tab;

    sol::table html = sv.create_table();
    const auto to_table = [](sol::this_state ts, const std::vector<std::string>& vals) {
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < vals.size(); ++i) {
            out[i + 1] = vals[i];
        }
        return out;
    };
    html["select"] = [to_table](sol::this_state ts, const std::string& h, const std::string& c) -> sol::object {
        return to_table(ts, html_select(h, c));
    };
    html["attr"] = [to_table](sol::this_state ts, const std::string& h, const std::string& c,
                              const std::string& a) -> sol::object { return to_table(ts, html_attr(h, c, a)); };
    // P8/P9: host.html.xpath(expr, html) -> string table; nil on bad expr.
    html["xpath"] = [](const std::string& expr, const std::string& doc, sol::this_state ts) -> sol::object {
        const auto vals = plugin::host_xpath::xpath(doc, expr);
        if (!vals) {
            return sol::make_object(ts, sol::lua_nil);
        }
        sol::table out(ts.lua_state(), sol::new_table());
        for (size_t i = 0; i < vals->size(); ++i) {
            out[i + 1] = (*vals)[i];
        }
        return sol::make_object(ts, out);
    };
    fs_tab["html"] = html;
}

} // namespace plugin::lua
