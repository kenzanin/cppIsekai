// Config core: layered resolve (default < ini < env < explicit flag) over the registry.
// All config knowledge lives in src/config/ (D1); header-only, no .cpp.
#pragma once

#include "registry.hpp"

#include <CLI/CLI.hpp>

#include <SimpleIni.h> // vendored 4.27; read-only here, comment-safe save lands in 2.2

#include <array>
#include <cstdlib>
#include <functional>
#include <string>

namespace config {

struct Config {
    int port;
    std::string proxy;
    std::string plugins_dir;
    std::int64_t fetch_timeout_ms;
    std::string fetch_profile;
    std::int64_t plugin_timeout_ops;
    std::int64_t cache_max_mb;
    std::string db;
    std::string log_file;
    std::string data_dir;
};

// Per-knob CLI state. Explicit = opt->count() > 0 (D7), never value comparison;
// unset flags leave value untouched so ini/env survive.
struct Overrides {
    std::array<std::string, kEntryCount> value; // last argument per knob, meaningful iff explicit
    std::array<CLI::Option*, kEntryCount> opt{};
};

// Binds every registry knob as a long option; second name rides CLI11's comma list.
inline void build_cli(CLI::App& app, Overrides& o) {
    for (std::size_t i = 0; i < kEntryCount; ++i) {
        const Entry& e = kEntries[i];
        std::string names = std::string("--") + e.flag;
        if ((e.alias != nullptr) && ((*e.alias) != 0)) {
            names += std::string(",--") + e.alias;
        }
        o.opt[i] = app.add_option(names, o.value[i], e.doc);
    }
}

using EnvLookup = std::function<const char*(const char*)>;

namespace detail {

// Lenient numeric read: empty/unparseable falls back to the registry default.
template <class T> inline T num_or(const std::string& s, const char* def) {
    try {
        return static_cast<T>(std::stoll(s));
    } catch (...) { // ponytail: bad numerics silently take the default; surface when 1.2 wires logging
    }
    return static_cast<T>(std::stoll(def)); // registry defaults are well-formed
}

// Resolved strings -> typed fields. Indexes must stay in lockstep with kEntries.
inline Config to_config(const std::array<std::string, kEntryCount>& v) {
    Config c;
    c.port = num_or<int>(v[0], kEntries[0].def);
    c.proxy = v[1];
    c.plugins_dir = v[2];
    c.fetch_timeout_ms = num_or<std::int64_t>(v[3], kEntries[3].def);
    c.fetch_profile = v[4];
    c.plugin_timeout_ops = num_or<std::int64_t>(v[5], kEntries[5].def);
    c.cache_max_mb = num_or<std::int64_t>(v[6], kEntries[6].def);
    c.db = v[7];
    c.log_file = v[8];
    c.data_dir = v[9];
    if (c.port <= 0) {
        c.port = num_or<int>(kEntries[0].def, kEntries[0].def); // design ceiling: port > 0
    }
    return c;
}

} // namespace detail

// Layered resolve. Missing/corrupt ini skips that layer (same as main.cpp today);
// env lookup injectable for tests; pass {} to run with no env at all.
inline Config resolve(
    const Overrides& flags, const std::string& ini_path,
    const EnvLookup& env = [](const char* n) { return std::getenv(n); }) {
    CSimpleIniA ini;
    const bool have_ini = ini.LoadFile(ini_path.c_str()) >= 0;

    std::array<std::string, kEntryCount> v;
    for (std::size_t i = 0; i < kEntryCount; ++i) {
        const Entry& e = kEntries[i];
        v[i] = e.def; // 1. built-in default
        if (have_ini) {
            if (const char* iv = ini.GetValue(e.section, e.key, nullptr)) {
                v[i] = iv; // 2. ini
            }
        }
        if (env) {
            if (const char* ev = env(e.env)) {
                v[i] = ev; // 3. CPPISEKAI_* env
            }
        }
        if ((flags.opt[i] != nullptr) && flags.opt[i]->count() > 0) {
            v[i] = flags.value[i]; // 4. explicit flag
        }
    }
    return detail::to_config(v);
}

} // namespace config
