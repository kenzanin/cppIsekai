#pragma once

/** \file plugin.hpp
 *  \brief Plugin discovery, sandboxing, host tools, ABI envelope dispatch.
 *
 * Layout: one folder per plugin under Config::dir with main.json (manifest),
 * the entry script (main.lua) and an optional logo file. Contract: host calls
 * handle({api, method, params}); the plugin answers {ok=true, data=...} or
 * {ok=false, error=...}. */

#include "fs.hpp" // plugin::fs_read -- confined read-only file helper (also drags <filesystem>/<fstream> for embedders)
#include <BS_thread_pool.hpp>
#include <curl/curl.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

struct Store;

namespace plugin {

/** \brief Bump when the envelope/param contract changes; plugins with a newer
 *  api are skipped at load (older ones run best-effort). */
constexpr int kHostApi = 1;

/** \brief Live default-profile cell (task 1.2): POST /config swaps the value
 *  through Host::set_default_profile; the runtimes read it per call. Points
 *  into Host::Impl -- stable for the Host's lifetime. */
using LiveProfile = std::atomic<std::shared_ptr<const std::string>>;

struct Config {
    std::string dir = "data/plugins";
    long fetch_timeout_ms = 30000;
    std::string fetch_profile = "chrome116";
    // Instruction budget for the Lua count hook, in ~instructions (hook fires
    // per 1000 vm instructions).
    long instruction_budget = 5000000;

    // Host-wide outbound proxy (ini [server] proxy); the fetch gate applies
    // it to every handle; empty = direct.
    std::string proxy;

    // Second scan root for metadata-only enrichment plugins (ini [paths]
    // enrichment_dir). Same runtimes and host tools as scraper plugins; their
    // capability is tagged `enrichment` and they never enter the scraper
    // registry (/plugins). Empty disables the enrichment scan.
    std::string enrichment_dir = "data/enrichment";
};

struct PluginInfo {
    std::string id, name, version, runtime, entry;
    std::vector<std::string> capabilities;
    int api{};
    std::string dir;
};

/** \brief Last finished call per plugin; in-memory, resets on restart. */
struct CallHealth {
    bool ok{true};
    std::string error; // empty when ok
    std::int64_t at{}; // unix seconds
};

struct Host; // fwd: Impl defined in plugin.cpp as plugin::Host::Impl

/** \brief Registry + runtime holder. One instance for the process; safe to
 *  call from pool threads. */
class Host {
public:
    explicit Host(Config conf);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    /** \brief Rescan dir, rebuild every runtime, upsert survivors into Store.
     *  Returns the number of plugins kept. A broken plugin logs a warning and
     *  never aborts the scan. */
    int load_all(Store& store);

    /** \brief Optional db-worker gate: when set, plugin `storage.*` calls run
     *  on this pool (Store is single-connection, db-worker only). Without it
     *  the calls run inline -- fine for single-threaded tests. */
    void use_db(BS::thread_pool<>* db);

    /** \brief ABI call. Dispatches on the manifest runtime; envelope, health
     *  bookkeeping and response shape are the runtime-agnostic core. Returns
     *  the response as JSON text:
     *  {"ok":true,"data":...} or {"ok":false,"error":"..."}. */
    std::string call(const std::string& id, const std::string& method, const std::string& params_json);

    /** \brief True when id is an enrichment source (capability `enrichment`). */
    bool is_enrichment(const std::string& id) const;

    /** \brief Enrichment call (task 1.3): kind ∈ `detail` | `covers`. Gates
     *  on the plugin's `enrichment` capability, routes through the shared
     *  `call` path (same envelope, health, proxy, cookie handling), and
     *  returns `{"ok":true,"data":...}` with a validated data object or
     *  `{"ok":false,"error":...}`. Never writes to the Store. */
    std::string enrich(const std::string& id, const std::string& title, const std::string& kind);

    /** \brief Loaded enrichment sources (id + display name), for the detail-page picker. */
    std::vector<std::pair<std::string, std::string>> enrichment_sources() const;

    /** \brief Last-call outcome for id; ok-when-unknown when never called. */
    [[nodiscard]] CallHealth health(const std::string& id) const;

    /** \brief Swap the live default profile (task 1.2); the config route
     *  calls this after a successful save. */
    void set_default_profile(std::string profile);

    /** \brief Current live default profile (plugin.fetch_profile). */
    [[nodiscard]] std::string default_profile() const;

    /** \brief Logo file path for id; empty when the plugin (or its logo) is
     *  absent. */
    [[nodiscard]] std::string logo_path(const std::string& id) const;

private:
    struct Impl; // fwd decl inside Host: defined as plugin::Host::Impl in plugin.cpp
    Config conf_;
    std::unique_ptr<Impl> impl_;
};

} // namespace plugin
