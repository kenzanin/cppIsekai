#pragma once
// Plugin discovery, Lua sandbox, host tools, ABI envelope dispatch.
// Layout: one folder per plugin under Config::dir with main.json (manifest),
// the entry script (main.lua) and an optional logo file.
// Contract: host calls handle({api, method, params}); the plugin answers
// {ok=true, data=...} or {ok=false, error=...}.
#include <BS_thread_pool.hpp>

#include <memory>
#include <string>
#include <vector>

struct Store;

namespace plugin {

// Bump when the envelope/param contract changes; plugins with a newer api are
// skipped at load (older ones run best-effort).
constexpr int kHostApi = 1;

struct Config {
    std::string dir = "data/plugins";
    long fetch_timeout_ms = 30000;
    std::string fetch_profile = "chrome116";
    // Instruction budget for the Lua count hook, in ~instructions (hook fires
    // per 1000 vm instructions).
    long instruction_budget = 5000000;
};

struct PluginInfo {
    std::string id, name, version, runtime, entry;
    std::vector<std::string> capabilities;
    int api{};
    std::string dir;
};

struct Host; // fwd: Impl defined in plugin.cpp as plugin::Host::Impl
// Registry + runtime holder. One instance for the process; safe to call from
// pool threads.
class Host {
public:
    explicit Host(Config conf);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // Rescan dir, rebuild every runtime, upsert survivors into Store.
    // Returns the number of plugins kept. A broken plugin logs a warning and
    // never aborts the scan.
    int load_all(Store& store);

    // Optional db-worker gate: when set, plugin `storage.*` calls are run on
    // this pool (Store is single-connection, db-worker only). Without it the
    // calls run inline — fine for single-threaded tests.
    void use_db(BS::thread_pool<>* db);

    // ABI call. Returns the response as JSON text:
    // {"ok":true,"data":...} or {"ok":false,"error":"..."}.
    std::string call(const std::string& id, const std::string& method, const std::string& params_json);

    // Logo file path for id; empty when the plugin (or its logo) is absent.
    [[nodiscard]] std::string logo_path(const std::string& id) const;

private:
    struct Impl; // fwd decl inside Host: defined as plugin::Host::Impl in plugin.cpp
    Config conf_;
    std::unique_ptr<Impl> impl_;
};

} // namespace plugin
