#include "plugin.hpp"

#include "store/store.hpp"

#include <glaze/glaze.hpp>
#include <sol/sol.hpp>

#include <curl/curl.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

// curl-impersonate ships the symbol but no header on this host (system curl
// headers predate it; see knowledge/references/dependency-api.md).
extern "C" CURLcode curl_easy_impersonate(CURL* easy, const char* client, unsigned clientscope);

namespace plugin {

namespace {

// ---------------------------------------------------------------------------
// json <-> lua bridges. glz::generic = variant<null,double,string,bool,vec,map>
// ---------------------------------------------------------------------------

std::string dump_json(const glz::generic& g) {
    if (auto s = glz::write_json(g))
        return std::move(*s);
    return "null";
}

std::string err_json(std::string msg) {
    glz::generic ok, err, root;
    ok.data = false;
    err.data = std::move(msg);
    auto& obj = root.data.emplace<glz::generic::object_t>();
    obj.insert({"ok", std::move(ok)});
    obj.insert({"error", std::move(err)});
    return dump_json(root);
}

sol::object json_to_sol(lua_State* L, const glz::generic& g) {
    sol::state_view sv(L);
    return std::visit(
        [&](auto&& v) -> sol::object {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_null_pointer_v<T>) {
                return sol::make_object(L, sol::nil);
            } else if constexpr (std::is_same_v<T, glz::generic::array_t>) {
                sol::table t = sol::table(L, sol::new_table());
                for (size_t i = 0; i < v.size(); ++i)
                    t[i + 1] = json_to_sol(L, v[i]);
                return sol::make_object(L, t);
            } else if constexpr (std::is_same_v<T, glz::generic::object_t>) {
                sol::table t = sol::table(L, sol::new_table());
                for (const auto& [k, val] : v)
                    t[k] = json_to_sol(L, val);
                return sol::make_object(L, t);
            } else {
                return sol::make_object(L, v); // double / string / bool
            }
        },
        g.data);
}

glz::generic sol_to_json(const sol::object& o, int depth = 0) {
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
            for (size_t i = 1; i <= n; ++i)
                arr.push_back(sol_to_json(t[i], depth + 1));
        } else {
            auto& obj = g.data.emplace<generic::object_t>();
            for (const auto& kv : t) {
                std::string key;
                if (kv.first.get_type() == sol::type::number)
                    key = std::to_string(static_cast<long long>(kv.first.as<double>()));
                else if (kv.first.get_type() == sol::type::string)
                    key = kv.first.as<std::string>();
                else
                    continue; // non-scalar key is not representable in JSON
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

// ---------------------------------------------------------------------------
// curl
// ---------------------------------------------------------------------------

struct FetchSink {
    std::string body;
    std::map<std::string, std::string> headers; // keys lowercased, last wins
};

size_t on_body(char* ptr, size_t size, size_t nmemb, void* ud) {
    static_cast<std::string*>(ud)->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t on_header(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(ud);
    std::string_view line(ptr, size * nmemb);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.remove_suffix(1);
    const auto colon = line.find(':');
    if (colon == std::string_view::npos)
        return size * nmemb; // status line or obs-fold continuation
    std::string key(line.substr(0, colon));
    std::string value(line.substr(colon + 1));
    auto trim = [](std::string& s) {
        const auto issp = [](unsigned char c) { return c == ' ' || c == '	'; };
        while (!s.empty() && issp(s.front()))
            s.erase(s.begin());
        while (!s.empty() && issp(s.back()))
            s.pop_back();
    };
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    trim(key);
    trim(value);
    if (!key.empty())
        (*headers)[key] = std::move(value);
    return size * nmemb;
}

using CurlPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

// ---------------------------------------------------------------------------
// per-call instruction budget; the hook is process-global per calling thread
// ---------------------------------------------------------------------------

thread_local long long t_budget_left = 0;

void count_hook(lua_State* L, lua_Debug*) {
    if ((t_budget_left -= 1000) <= 0)
        luaL_error(L, "plugin instruction budget exhausted");
}

// ---------------------------------------------------------------------------
// sandbox + host tools. Only base/string/table/math are opened; json is
// host-provided (glaze) and io a read-only two-function shim. os, debug and
// package are never opened.
// ---------------------------------------------------------------------------

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

void install_host_tools(lua_State* L, const std::string& id, const Config& conf) {
    sol::state_view sv(L);

    // Routed through the default logger, whose sinks include the queue sink
    // feeding the log page (wired in main).
    sol::table log = sv.create_named_table("log");
    log["info"] = [id](const std::string& msg) { spdlog::info("[plugin:{}] {}", id, msg); };
    log["error"] = [id](const std::string& msg) { spdlog::error("[plugin:{}] {}", id, msg); };

    sv.set_function("now", [] { return static_cast<long long>(std::time(nullptr)); });

    // ponytail: io shim is read-only via istream; widen only when a plugin
    // needs incremental reads (io.lines multi-arg / formats).
    sol::table io = sv.create_named_table("io");
    io["read"] = []() -> std::optional<std::string> {
        std::string line;
        if (std::getline(std::cin, line))
            return line;
        return std::nullopt;
    };
    io["lines"] = [](const std::string& path, sol::this_state ts) -> sol::object {
        auto f = std::make_shared<std::ifstream>(path);
        if (!*f)
            throw std::runtime_error("io.lines: cannot open " + path);
        lua_State* L = ts;
        return sol::make_object(L, [f]() -> std::optional<std::string> {
            std::string line;
            if (std::getline(*f, line))
                return line;
            return std::nullopt; // iterator ends; the file closes with the closure
        });
    };

    sol::table json = sv.create_named_table("json");
    json["encode"] = [](const sol::object& o) { return dump_json(sol_to_json(o)); };
    json["decode"] = [](const std::string& s, sol::this_state ts) -> sol::object {
        glz::generic g;
        if (auto ec = glz::read_json(g, s))
            throw std::runtime_error("json.decode: invalid JSON");
        lua_State* L = ts;
        return json_to_sol(L, g);
    };

    // Blocking fetch on the calling (plugin-pool) worker; curl's own timeout
    // guards the network half, the instruction hook the CPU half.
    sol::table http = sv.create_named_table("http");
    http["fetch"] =
        [id, timeout_ms = conf.fetch_timeout_ms, profile = conf.fetch_profile](
            const std::string& url, const sol::optional<sol::table>& opts,
            sol::this_state ts) -> std::pair<sol::object, std::optional<std::string>> {
        const auto fail = [&](std::string why) {
            spdlog::warn("[plugin:{}] fetch {} failed: {}", id, url, why);
            return std::make_pair(sol::make_object(ts, sol::lua_nil), std::optional(std::move(why)));
        };

        long ms = timeout_ms;
        std::string profile_name = profile;
        std::string accept_header; // optional per-call Accept override
        std::string ua_header;     // optional per-call User-Agent override
        if (opts) {
            if (sol::object v = (*opts)["timeout_ms"]; v.get_type() == sol::type::number)
                ms = static_cast<long>(v.as<double>());
            if (sol::object v = (*opts)["profile"]; v.get_type() == sol::type::string)
                profile_name = v.as<std::string>();
            if (sol::object v = (*opts)["accept"]; v.get_type() == sol::type::string)
                accept_header = v.as<std::string>();
            if (sol::object v = (*opts)["user_agent"]; v.get_type() == sol::type::string)
                ua_header = v.as<std::string>();
        }

        CurlPtr easy(curl_easy_init(), &curl_easy_cleanup);
        if (!easy)
            return fail("curl init failed");

        FetchSink sink;
        curl_slist* hdrs = nullptr;
        if (!accept_header.empty())
            hdrs = curl_slist_append(hdrs, ("Accept: " + accept_header).c_str());
        if (!ua_header.empty())
            hdrs = curl_slist_append(hdrs, ("User-Agent: " + ua_header).c_str());
        curl_easy_setopt(easy.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(easy.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy.get(), CURLOPT_WRITEFUNCTION, on_body);
        curl_easy_setopt(easy.get(), CURLOPT_WRITEDATA, &sink.body);
        curl_easy_setopt(easy.get(), CURLOPT_HEADERFUNCTION, on_header);
        curl_easy_setopt(easy.get(), CURLOPT_HEADERDATA, &sink.headers);
        curl_easy_setopt(easy.get(), CURLOPT_TIMEOUT_MS, std::max(1L, ms));
        curl_easy_setopt(easy.get(), CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
        if (hdrs)
            curl_easy_setopt(easy.get(), CURLOPT_HTTPHEADER, hdrs);
        if (curl_easy_impersonate(easy.get(), profile_name.c_str(), 1) != CURLE_OK)
            return fail("impersonate profile '" + profile_name + "' unsupported");
        // ponytail: this impersonate build intermittently fails TLS handshake
        // (ECONNRESET) against MangaDex's LB; one fresh-handle retry removes it.
        CURLcode rc = CURLE_OK;
        for (int attempt = 0; attempt < 2; ++attempt) {
            rc = curl_easy_perform(easy.get());
            if (rc == CURLE_OK || rc != CURLE_SSL_CONNECT_ERROR)
                break;
            easy.reset(curl_easy_init());
            if (!easy)
                return fail("curl re-init failed");
            curl_easy_setopt(easy.get(), CURLOPT_URL, url.c_str());
            curl_easy_setopt(easy.get(), CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(easy.get(), CURLOPT_WRITEFUNCTION, on_body);
            curl_easy_setopt(easy.get(), CURLOPT_WRITEDATA, &sink.body);
            curl_easy_setopt(easy.get(), CURLOPT_HEADERFUNCTION, on_header);
            curl_easy_setopt(easy.get(), CURLOPT_HEADERDATA, &sink.headers);
            curl_easy_setopt(easy.get(), CURLOPT_TIMEOUT_MS, std::max(1L, ms));
            curl_easy_setopt(easy.get(), CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
            curl_easy_impersonate(easy.get(), profile_name.c_str(), 1);
        }
        if (rc != CURLE_OK)
            return fail(curl_easy_strerror(rc));

        long code = 0;
        curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &code);

        sol::table out = sol::table(ts.lua_state(), sol::new_table());
        out["status"] = static_cast<long long>(code);
        out["body"] = sink.body;
        sol::table hh = sol::table(ts.lua_state(), sol::new_table());
        for (const auto& [k, v] : sink.headers)
            hh[k] = v;
        out["headers"] = sol::make_object(ts, hh);
        return {sol::make_object(ts, out), std::nullopt};
    };
} // anonymous namespace

struct Runtime {
    PluginInfo info;
    std::unique_ptr<lua_State, decltype(&lua_close)> L{nullptr, &lua_close};
    std::mutex mu; // one call at a time per state
};

// Manifest; optional members may be absent, the rest are required by glaze.
struct Manifest {
    std::string name, version, runtime;
    std::optional<std::string> id, entry;
    std::optional<std::vector<std::string>> capabilities;
    int api{};
};
} // namespace plugin

struct plugin::Host::Impl {
    Config conf;
    std::mutex mu;
    std::map<std::string, std::shared_ptr<Runtime>> plugins;
};

Host::Host(Config conf) : conf_(conf), impl_(std::make_unique<Host::Impl>()) {
    impl_->conf = std::move(conf);
}

Host::~Host() = default;

int Host::load_all(Store& store) {
    std::map<std::string, std::shared_ptr<Runtime>> fresh;

    std::error_code ec;
    if (!fs::exists(fs::path(conf_.dir), ec)) {
        spdlog::warn("plugin dir '{}' missing; no plugins loaded", conf_.dir);
        const std::lock_guard lock(impl_->mu);
        impl_->plugins.clear();
        return 0;
    }

    for (const auto& item : fs::directory_iterator(fs::path(conf_.dir))) {
        if (!item.is_directory())
            continue;
        const std::string folder = item.path().filename().string();

        std::ifstream in(item.path() / "main.json");
        if (!in) {
            spdlog::warn("plugin {}: no main.json, skipped", folder);
            continue;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        const std::string text = buf.str();

        Manifest m;
        if (auto e = glz::read<glz::opts{.error_on_unknown_keys = false}>(m, text)) {
            spdlog::warn("plugin {}: manifest rejected (missing/invalid fields)", folder);
            continue;
        }

        // Duplicate ids: first in scan order wins.
        Runtime rt;
        rt.info.id = m.id.value_or(folder);
        if (fresh.contains(rt.info.id)) {
            spdlog::warn("plugin {}: duplicate id, keeping the first", rt.info.id);
            continue;
        }
        rt.info.name = m.name;
        rt.info.version = m.version;
        rt.info.runtime = m.runtime;
        rt.info.api = m.api;
        rt.info.entry = m.entry.value_or("main.lua");
        if (m.capabilities)
            rt.info.capabilities = *m.capabilities;
        rt.info.dir = item.path().string();

        if (m.runtime != "lua") {
            spdlog::warn("plugin {}: runtime '{}' not loadable (host loads: lua)", rt.info.id, m.runtime);
            continue;
        }
        if (m.api > kHostApi) {
            spdlog::warn("plugin {}: manifest api {} > host api {}, skipped", rt.info.id, m.api, kHostApi);
            continue;
        }
        if (!fs::exists(fs::path(rt.info.dir) / rt.info.entry)) {
            spdlog::warn("plugin {}: entry {} not found", rt.info.id, rt.info.entry);
            continue;
        }

        lua_State* L = luaL_newstate();
        if (!L) {
            spdlog::warn("plugin {}: lua state allocation failed", rt.info.id);
            continue;
        }
        rt.L = {L, &lua_close};
        open_sandbox(L);
        install_host_tools(L, rt.info.id, conf_);
        // Entry scripts return nothing; run them, check validity explicitly.
        const sol::protected_function_result script =
            sol::state_view(L).safe_script_file((fs::path(rt.info.dir) / rt.info.entry).string());
        if (!script.valid()) {
            const sol::error e = script;
            spdlog::warn("plugin {}: entry script failed: {}", rt.info.id, e.what());
            continue;
        }
        // The chunk may leave a nil result on the stack; balance it so the
        // protected-call path below never sees a stray value.
        lua_pop(L, lua_gettop(L));

        std::string caps;
        for (size_t i = 0; i < rt.info.capabilities.size(); ++i)
            caps += (i ? "," : "") + rt.info.capabilities[i];
        store.upsert_plugin(rt.info.id, rt.info.name, rt.info.version, caps);
        spdlog::info("plugin loaded: {} v{} (api {}, caps: {})", rt.info.id, rt.info.version, rt.info.api,
                     caps.empty() ? std::string("none") : caps);

        auto shared = std::make_shared<Runtime>();
        shared->info = std::move(rt.info);
        shared->L = std::move(rt.L);
        fresh.emplace(shared->info.id, std::move(shared));
    }

    const int kept = static_cast<int>(fresh.size());
    const std::lock_guard lock(impl_->mu);
    impl_->plugins = std::move(fresh);
    return kept;
}

std::string Host::call(const std::string& id, const std::string& method, const std::string& params_json) {
    std::shared_ptr<Runtime> rt;
    {
        const std::lock_guard lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end())
            rt = it->second;
    }
    if (!rt)
        return err_json("no plugin '" + id + "'");

    const std::lock_guard call_lock(rt->mu);
    lua_State* L = rt->L.get();
    sol::state_view sv(L);

    sol::object params_obj;
    if (params_json.empty()) {
        params_obj = sol::make_object(L, sol::table(L, sol::new_table()));
    } else {
        glz::generic g;
        if (auto ec = glz::read_json(g, params_json))
            return err_json("bad params json");
        params_obj = json_to_sol(L, g);
    }

    const sol::object h = sv["handle"];
    if (h.get_type() != sol::type::function)
        return err_json("no handle() defined");

    sol::table env = sol::table(L, sol::new_table()); // constructed directly on L
    env["api"] = kHostApi;
    env["method"] = method;
    env["params"] = params_obj;

    t_budget_left = conf_.instruction_budget;
    lua_sethook(L, count_hook, LUA_MASKCOUNT, 1000);
    const sol::protected_function_result res = sol::protected_function(h)(env);
    lua_sethook(L, nullptr, 0, 0);
    const bool timed_out = t_budget_left <= 0;

    if (!res.valid()) {
        const sol::error e = res;
        if (timed_out) {
            spdlog::warn("[plugin:{}] {} timed out", id, method);
            return err_json("timeout: instruction budget exhausted");
        }
        spdlog::warn("[plugin:{}] {} failed: {}", id, method, e.what());
        return err_json(e.what());
    }

    const sol::object r = res;
    if (r.get_type() != sol::type::table) {
        spdlog::warn("[plugin:{}] {}: malformed response (non-table)", id, method);
        return err_json("malformed response: not a table");
    }
    const sol::table t = r;
    const sol::object okv = t["ok"];
    if (okv.get_type() != sol::type::boolean) {
        spdlog::warn("[plugin:{}] {}: malformed response (missing ok)", id, method);
        return err_json("malformed response: missing ok");
    }
    if (!okv.as<bool>()) {
        const sol::object ev = t["error"];
        const std::string msg =
            ev.get_type() == sol::type::string ? ev.as<std::string>() : std::string("plugin error");
        spdlog::warn("[plugin:{}] {} returned error: {}", id, method, msg);
        return err_json(msg);
    }
    return "{\"ok\":true,\"data\":" + dump_json(sol_to_json(t["data"])) + "}";
}

std::string Host::logo_path(const std::string& id) const {
    std::shared_ptr<Runtime> rt;
    {
        const std::lock_guard lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end())
            rt = it->second;
    }
    if (!rt)
        return "";
    for (const char* name : {"logo.png", "logo.svg", "logo.jpg", "logo.jpeg", "logo.avif"}) {
        const fs::path p = fs::path(rt->info.dir) / name;
        if (fs::exists(p))
            return p.string();
    }
    return "";
}

} // namespace plugin
