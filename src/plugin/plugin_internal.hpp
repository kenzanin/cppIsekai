#pragma once

/** \file plugin_internal.hpp
 *  \brief Internal seams shared by the plugin Host TUs (load/call/core).
 *  Not a public header; plugin.hpp stays the surface.
 */

#include "fs.hpp"
#include "http_client.hpp"
#include "lua/bridge.hpp"
#include "plugin.hpp"
#include "plugin/js/js_rt.hpp"
#include "plugin/wasm/wasm_rt.hpp"
#include "runtime_common.hpp"

#include <glaze/glaze.hpp>
#include <sol/sol.hpp>

#include <curl/curl.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace plugin {

// Per-plugin runtime holder, variant-ready: the loader engages the member
// matching info.runtime ("lua" today; wasm/js add their state beside it and a
// dispatch arm in Host::call -- tasks 2.1/3.2). Health bookkeeping, the call
// mutex, the request envelope and the response-shape rules stay in
// Host::call, shared by every backend.
struct Runtime {
    PluginInfo info;
    std::mutex mu;    // one call at a time per runtime, whatever the backend
    glz::generic cfg; // parsed config.json; meaningful only when has_cfg
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
    std::optional<std::string> id, entry, website;
    std::optional<std::vector<std::string>> capabilities;
    int api{};
};

struct Host::Impl {
    Config conf;
    LiveProfile live_profile;
    std::mutex mu;
    std::map<std::string, std::shared_ptr<Runtime>> plugins;
    BS::thread_pool<>* db = nullptr;          // db-worker gate for storage.*, set via use_db
    std::map<std::string, CallHealth> health; // last call outcome per plugin id
};

std::string err_json(std::string msg);
// The wasm env.http_fetch service: the same option names the lua front takes,
// JSON-encoded; the lua-shaped result ({status, body, headers}) as JSON back.
// nullopt = fetch failure (the import reports -1). Needs conf.proxy; the
// timer note: lives in plugin_load.cpp (first consumer was load_all).
wasm::HttpFetch make_wasm_fetch(const std::string& id, const Config& conf, const LiveProfile& live_profile);

RawReply lua_call_entry(const Runtime& rt, const std::string& method, long instruction_budget,
                        const glz::generic& envelope);

// The response-shape gate, shared by every backend: {"ok":bool} plus the
// ok=true "data" contract, exactly as the core pinned it. Returns the wire
// response; nullopt with `error` set when the shape is rejected (the warning
// carries plugin/method context).
std::optional<std::string> checked_reply(const glz::generic& reply, const std::string& id, const std::string& method,
                                         std::string& error);

} // namespace plugin
