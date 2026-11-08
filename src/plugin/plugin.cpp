#include "plugin.hpp"
#include "fs.hpp"
#include "lua/bridge.hpp"
#include "http_client.hpp"
#include "plugin/wasm/wasm_rt.hpp"
#include "plugin/js/js_rt.hpp"

#include "store/store.hpp"

#include <algorithm>
#include <glaze/glaze.hpp>
#include <sol/sol.hpp>

#include <curl/curl.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace plugin {

// The lua seam lives in plugin/lua; the shared sync curl client in http_client.
using lua::json_to_sol;
using lua::sol_to_json;
using lua::open_sandbox;
using lua::install_host_tools;

namespace {

// ---------------------------------------------------------------------------
// json <-> lua bridges. glz::generic = variant<null,double,string,bool,vec,map>
// ---------------------------------------------------------------------------

std::string err_json(std::string msg) {
    glz::generic ok;
    glz::generic err;
    glz::generic root;
    ok.data = false;
    err.data = std::move(msg);
    auto& obj = root.data.emplace<glz::generic::object_t>();
    obj.insert({"ok", std::move(ok)});
    obj.insert({"error", std::move(err)});
    return dump_json(root);
}

// The wasm env.http_fetch service: the same option names the lua front takes,
// JSON-encoded; the lua-shaped result ({status, body, headers}) as JSON back.
// nullopt = fetch failure (the import reports -1).
wasm::HttpFetch make_wasm_fetch(const std::string& id, const Config& conf, const LiveProfile& live_profile) {
    return [id, timeout_ms = conf.fetch_timeout_ms, live = &live_profile, proxy = conf.proxy](
               const std::string& url, const std::string& opts_json) -> std::optional<std::string> {
        long ms = timeout_ms;
        std::string live_default;
        if (auto p = live->load()) {
            live_default = *p;
        }
        std::string profile_name = live_default;
        std::string method = "GET";
        std::string body;
        std::string accept_header;
        std::string ua_header;
        std::vector<std::pair<std::string, std::string>> extra_headers;

        glz::generic o;
        if (auto ec = glz::read_json(o, opts_json); !ec) {
            if (const auto* obj = std::get_if<glz::generic::object_t>(&o.data)) {
                const auto num = [&](const char* key, long& dst) {
                    if (const auto* v = member(obj, key)) {
                        if (const auto* d = std::get_if<double>(&v->data)) {
                            dst = static_cast<long>(*d);
                        }
                    }
                };
                const auto str = [&](const char* key, std::string& dst) {
                    if (const auto* v = member(obj, key)) {
                        if (const auto* s = std::get_if<std::string>(&v->data)) {
                            dst = *s;
                        }
                    }
                };
                num("timeout_ms", ms);
                str("profile", profile_name);
                str("method", method);
                str("body", body);
                str("accept", accept_header);
                str("user_agent", ua_header);
                if (const auto* h = member(obj, "headers")) {
                    if (const auto* ho = std::get_if<glz::generic::object_t>(&h->data)) {
                        for (const auto& [k, v] : *ho) {
                            if (const auto* s = std::get_if<std::string>(&v.data)) {
                                extra_headers.emplace_back(k, *s);
                            }
                        }
                    }
                }
            }
        }

        profile_name = checked_profile(std::move(profile_name), live_default);
        const FetchResult fr = fetch_sync(url, ms, profile_name, method, body, accept_header, ua_header,
                                          extra_headers, proxy);
        if (!fr.error.empty()) {
            spdlog::warn("[plugin:{}] fetch {} failed: {}", id, url, fr.error);
            return std::nullopt;
        }
        glz::generic out;
        auto& obj = out.data.emplace<glz::generic::object_t>();
        glz::generic status;
        status.data = static_cast<double>(fr.status);
        obj.insert({"status", std::move(status)});
        glz::generic body_v;
        body_v.data = std::move(fr.body);
        obj.insert({"body", std::move(body_v)});
        glz::generic headers;
        auto& hh = headers.data.emplace<glz::generic::object_t>();
        for (auto&& [k, v] : fr.headers) {
            glz::generic val;
            val.data = v;
            hh.insert({k, std::move(val)});
        }
        obj.insert({"headers", std::move(headers)});
        return dump_json(out);
    };
}

// per-call instruction budget; the hook is process-global per calling thread
// --------------------------------------------------------------------------- // anonymous namespace

// Per-plugin runtime holder, variant-ready: the loader engages the member
// matching info.runtime ("lua" today; wasm/js add their state beside it and a
// dispatch arm in Host::call -- tasks 2.1/3.2). Health bookkeeping, the call
// mutex, the request envelope and the response-shape rules stay in
// Host::call, shared by every backend.
struct Runtime {
    PluginInfo info;
    std::mutex mu; // one call at a time per runtime, whatever the backend
    glz::generic cfg;  // parsed config.json; meaningful only when has_cfg
    bool has_cfg = false;

    // lua backend (info.runtime == "lua")
    std::unique_ptr<lua_State, decltype(&lua_close)> lua{nullptr, &lua_close};

    // wasm backend (info.runtime == "wasm"): loaded module + host services.
    std::unique_ptr<wasm::Instance> wasm;

    // js backend (info.runtime == "js"): quickjs realm + host globals.
    std::unique_ptr<js::Instance> js;
};

// Manifest; optional members may be absent, the rest are required by glaze.
struct Manifest {
    std::string name, version, runtime;
    std::optional<std::string> id, entry;
    std::optional<std::vector<std::string>> capabilities;
    int api{};
};

// ---------------------------------------------------------------------------
// Runtime seams (design D2: JSON envelope in, raw reply out). Host::call owns
// everything shared -- health, call mutex, envelope, response shape; a seam
// owns the runtime-specific rest and answers for its own invocation errors.
// ---------------------------------------------------------------------------

// Raw reply handed back to the core: the plugin's return value marshaled
// as-is, shape-checked later (plugin::RawReply, runtime_common.hpp). When the
// entry never produced one, invoked is false and error carries the
// host-side diagnostic (already logged).

// lua seam: materialize the envelope as lua values, invoke handle, marshal
// the reply back to JSON. Lua-level failures (exception, budget) are logged
// here; the reply value itself always reaches the core's shape check.
thread_local long long t_budget_left = 0;

void count_hook(lua_State* L, lua_Debug* /*unused*/) {
    if ((t_budget_left -= 1000) <= 0) {
        luaL_error(L, "plugin instruction budget exhausted");
    }
}

RawReply lua_call_entry(const Runtime& rt, const std::string& method, long instruction_budget,
                        const glz::generic& envelope) {
    RawReply out;
    lua_State* L = rt.lua.get();
    sol::state_view sv(L);

    const sol::object env = json_to_sol(L, envelope);
    const sol::object h = sv["handle"];
    if (h.get_type() != sol::type::function) {
        out.error = "no handle() defined";
        return out;
    }

    t_budget_left = instruction_budget;
    lua_sethook(L, count_hook, LUA_MASKCOUNT, 1000);
    const sol::protected_function_result res = sol::protected_function(h)(env);
    lua_sethook(L, nullptr, 0, 0);
    const bool timed_out = t_budget_left <= 0;

    if (!res.valid()) {
        const sol::error e = res;
        if (timed_out) {
            spdlog::warn("[plugin:{}] {} timed out", rt.info.id, method);
            out.error = "timeout: instruction budget exhausted";
            return out;
        }
        spdlog::warn("[plugin:{}] {} failed: {}", rt.info.id, method, e.what());
        out.error = e.what();
        return out;
    }

    out.invoked = true;
    const sol::object r = res;
    // depth -1: reply members marshal where the data value always has, so the
    // wrapper level does not eat one rung of the cyclic-table depth budget.
    out.reply = sol_to_json(r, -1);
    return out;
}

// The response-shape gate, shared by every backend: {"ok":bool} plus the
// ok=true "data" contract, exactly as the core pinned it. Returns the wire
// response; nullopt with `error` set when the shape is rejected (the warning
// carries plugin/method context).
std::optional<std::string> checked_reply(const glz::generic& reply, const std::string& id, const std::string& method,
                                         std::string& error) {
    const auto* obj = std::get_if<glz::generic::object_t>(&reply.data);
    if (obj == nullptr) {
        spdlog::warn("[plugin:{}] {}: malformed response (non-table)", id, method);
        error = "malformed response: not a table";
        return std::nullopt;
    }
    const glz::generic* okv = member(obj, "ok");
    if (okv == nullptr || std::get_if<bool>(&okv->data) == nullptr) {
        spdlog::warn("[plugin:{}] {}: malformed response (missing ok)", id, method);
        error = "malformed response: missing ok";
        return std::nullopt;
    }
    if (!*std::get_if<bool>(&okv->data)) {
        const glz::generic* ev = member(obj, "error");
        const bool is_str = ev != nullptr && std::get_if<std::string>(&ev->data) != nullptr;
        const std::string msg = is_str ? std::get<std::string>(ev->data) : std::string("plugin error");
        spdlog::warn("[plugin:{}] {} returned error: {}", id, method, msg);
        error = msg;
        return std::nullopt;
    }
    static const glz::generic null_v;
    const glz::generic* data = member(obj, "data");
    return R"({"ok":true,"data":)" + dump_json(data ? *data : null_v) + "}";
}
} // namespace

struct plugin::Host::Impl {
    Config conf;
    LiveProfile live_profile;
    std::mutex mu;
    std::map<std::string, std::shared_ptr<Runtime>> plugins;
    BS::thread_pool<>* db = nullptr; // db-worker gate for storage.*, set via use_db
    std::map<std::string, CallHealth> health; // last call outcome per plugin id
};

Host::Host(Config conf) : conf_(conf), impl_(std::make_unique<Host::Impl>()) {
    impl_->conf = std::move(conf);
    impl_->live_profile = std::make_shared<const std::string>(conf_.fetch_profile);
}

Host::~Host() = default;

void Host::use_db(BS::thread_pool<>* db) {
    impl_->db = db;
}

void Host::set_default_profile(std::string profile) {
    impl_->live_profile = std::make_shared<const std::string>(std::move(profile));
}

std::string Host::default_profile() const {
    if (auto p = impl_->live_profile.load()) {
        return *p;
    }
    return conf_.fetch_profile;
}

int Host::load_all(Store& store) {
    std::map<std::string, std::shared_ptr<Runtime>> fresh;

    // Storage closures: same gate for every runtime this scan produces.
    Store* sp = &store;
    BS::thread_pool<>* dbp = impl_->db;
    StateGet sget = [sp, dbp](const std::string& pid, const std::string& key) -> std::optional<std::string> {
        if (dbp != nullptr) {
            return dbp->submit_task([&] { return sp->state_get(pid, key); }).get();
        }
        return sp->state_get(pid, key); // single-threaded tests
    };
    StateSet sset = [sp, dbp](const std::string& pid, const std::string& key, const std::string& val) {
        if (dbp != nullptr) {
            dbp->submit_task([&] { sp->state_set(pid, key, val); }).get();
            return;
        }
        sp->state_set(pid, key, val);
    };

    std::error_code ec;
    if (!fs::exists(fs::path(conf_.dir), ec)) {
        spdlog::warn("plugin dir '{}' missing; no plugins loaded", conf_.dir);
        const std::scoped_lock lock(impl_->mu);
        impl_->plugins.clear();
        return 0;
    }

    for (const auto& item : fs::directory_iterator(fs::path(conf_.dir))) {
        if (!item.is_directory()) {
            continue;
        }
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
        const char* default_ext = "lua";
        if (m.runtime == "wasm") {
            default_ext = "wasm";
        } else if (m.runtime == "js") {
            default_ext = "js";
        }
        rt.info.entry = m.entry.value_or(std::string("main.") + default_ext);
        if (m.capabilities) {
            rt.info.capabilities = *m.capabilities;
        }
        rt.info.dir = item.path().string();

        // User config lives beside the manifest, is never read by anyone else,
        // and is optional: absent/invalid -> plugin runs with no config.
        if (std::ifstream cf(fs::path(rt.info.dir) / "config.json"); cf) {
            std::stringstream ss;
            ss << cf.rdbuf();
            if (auto ec = glz::read_json(rt.cfg, std::move(ss).str())) {
                spdlog::warn("plugin {}: config.json invalid ({})", rt.info.id, glz::format_error(ec));
            }
            else {
                rt.has_cfg = true;
            }
        }

        const bool is_lua = m.runtime == "lua";
        const bool is_wasm = m.runtime == "wasm";
        const bool is_js = m.runtime == "js";
        if (!is_lua && !is_wasm && !is_js) {
            spdlog::warn("plugin {}: runtime '{}' not loadable (host loads: lua, wasm, js)", rt.info.id, m.runtime);
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

        if (is_lua) {
            lua_State* L = luaL_newstate();
            if (L == nullptr) {
                spdlog::warn("plugin {}: lua state allocation failed", rt.info.id);
                continue;
            }
            rt.lua = {L, &lua_close};
            open_sandbox(L);
            install_host_tools(L, rt.info.id, conf_, rt.cfg, rt.has_cfg, sget, sset, rt.info.dir,
                               impl_->live_profile);
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
        } else if (is_js) {
            // js: entry is source text; the seam owns the realm and globals.
            std::ifstream jf(fs::path(rt.info.dir) / rt.info.entry, std::ios::binary);
            if (!jf) {
                spdlog::warn("plugin {}: js entry {} unreadable", rt.info.id, rt.info.entry);
                continue;
            }
            std::stringstream jbuf;
            jbuf << jf.rdbuf();
            js::Loaded loaded = js::load(rt.info.id, rt.info.dir, std::move(jbuf).str(),
                                         wasm::Services{sget, sset, make_wasm_fetch(rt.info.id, conf_, impl_->live_profile)});
            if (!loaded.instance) {
                spdlog::warn("plugin {}: js load failed: {}", rt.info.id, loaded.error);
                continue;
            }
            rt.js = std::move(loaded.instance);
        } else {
            // wasm: entry is a prebuilt .wasm image; the seam owns load/link.
            std::ifstream wf(fs::path(rt.info.dir) / rt.info.entry, std::ios::binary);
            if (!wf) {
                spdlog::warn("plugin {}: wasm entry {} unreadable", rt.info.id, rt.info.entry);
                continue;
            }
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(wf), {}};
            wasm::Services services{sget, sset, make_wasm_fetch(rt.info.id, conf_, impl_->live_profile)};
            wasm::Loaded loaded = wasm::load(rt.info.id, rt.info.dir, std::move(bytes), std::move(services));
            if (!loaded.instance) {
                spdlog::warn("plugin {}: wasm load failed: {}", rt.info.id, loaded.error);
                continue;
            }
            rt.wasm = std::move(loaded.instance);
        }

        std::string caps;
        for (size_t i = 0; i < rt.info.capabilities.size(); ++i) {
            caps += ((i != 0u) ? "," : "") + rt.info.capabilities[i];
        }
        store.upsert_plugin(rt.info.id, rt.info.name, rt.info.version, caps, rt.info.runtime);
        spdlog::info("plugin loaded: {} v{} (api {}, caps: {})", rt.info.id, rt.info.version, rt.info.api,
                     caps.empty() ? std::string("none") : caps);

        auto shared = std::make_shared<Runtime>();
        shared->info = std::move(rt.info);
        shared->lua = std::move(rt.lua);
        shared->wasm = std::move(rt.wasm);
        shared->js = std::move(rt.js);
        fresh.emplace(shared->info.id, std::move(shared));
    }

    const int kept = static_cast<int>(fresh.size());
    const std::scoped_lock lock(impl_->mu);
    impl_->plugins = std::move(fresh);
    return kept;
}

std::string Host::call(const std::string& id, const std::string& method, const std::string& params_json) {
    std::shared_ptr<Runtime> rt;
    {
        const std::scoped_lock lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end()) {
            rt = it->second;
        }
    }
    if (!rt) {
        return err_json("no plugin '" + id + "'");
    }

    // Health bookkeeping: one entry per finished call (design: in-memory).
    const auto record = [&](bool ok, std::string err) {
        const std::scoped_lock lock(impl_->mu);
        auto& h = impl_->health[id];
        h.ok = ok;
        h.error = std::move(err);
        h.at = std::time(nullptr);
    };
    const auto fail = [&](std::string msg) {
        record(false, msg);
        return err_json(std::move(msg));
    };

    const std::scoped_lock call_lock(rt->mu);

    // Runtime-agnostic request envelope; each backend marshals this itself.
    glz::generic envelope;
    auto& env = envelope.data.emplace<glz::generic::object_t>();
    glz::generic api_v;
    api_v.data = static_cast<double>(kHostApi);
    env["api"] = std::move(api_v);
    glz::generic method_v;
    method_v.data = method;
    env["method"] = std::move(method_v);
    if (params_json.empty()) {
        glz::generic empty_obj;
        empty_obj.data.emplace<glz::generic::object_t>();
        env["params"] = std::move(empty_obj);
    } else {
        glz::generic params;
        if (auto ec = glz::read_json(params, params_json)) {
            return fail("bad params json");
        }
        env["params"] = std::move(params);
    }

    RawReply r;
    if (rt->lua) {
        r = lua_call_entry(*rt, method, conf_.instruction_budget, envelope);
    } else if (rt->wasm) {
        r = wasm::invoke(*rt->wasm, dump_json(envelope));
    } else if (rt->js) {
        r = js::invoke(*rt->js, dump_json(envelope));
    } else {
        return fail("runtime '" + rt->info.runtime + "' not supported");
    }
    if (!r.invoked) {
        return fail(r.error);
    }
    std::string shape_err;
    if (std::optional<std::string> res = checked_reply(r.reply, id, method, shape_err)) {
        record(true, "");
        return *res;
    }
    return fail(std::move(shape_err));
}

CallHealth Host::health(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->health.find(id);
    return it != impl_->health.end() ? it->second : CallHealth{};
}

std::string Host::logo_path(const std::string& id) const {
    std::shared_ptr<Runtime> rt;
    {
        const std::scoped_lock lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end()) {
            rt = it->second;
        }
    }
    if (!rt) {
        return "";
    }
    for (const char* name : {"logo.png", "logo.svg", "logo.jpg", "logo.jpeg", "logo.avif"}) {
        const fs::path p = fs::path(rt->info.dir) / name;
        if (fs::exists(p)) {
            return p.string();
        }
    }
    return "";
}

} // namespace plugin
