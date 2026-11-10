#pragma once

/** \file config.hpp
 *  \brief Config core: layered resolve (default < ini < env < explicit flag)
 *  over the registry. All config knowledge lives in src/config/ (D1);
 *  header-only, no .cpp. */

#include "registry.hpp"

#include <CLI/CLI.hpp>
#include <glaze/glaze.hpp>

#include <SimpleIni.h> // vendored 4.27; load is read-only, save() below is the comment-safe merge

#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace config {

struct Config {
    int port;
    std::string proxy;
    std::string plugins_dir;
    std::int64_t fetch_timeout_ms;
    std::string fetch_profile;
    std::int64_t plugin_timeout_ops;
    std::int64_t cache_max_mb;
    std::string image_format;    // [cache] image_format: webp|avif|jxl|none
    std::int64_t cover_max_px;   // [cache] cover_max_px: cover resize cap
    std::string db;
    std::string log_file;
    std::string data_dir;
    std::string log_level; // [log] level: trace|debug|info|warning|error
};

// Per-knob CLI state. Explicit = opt->count() > 0 (D7), never value comparison;
// unset flags leave value untouched so ini/env survive.
struct Overrides {
    std::array<std::string, kEntryCount> value; // last argument per knob, meaningful iff explicit
    std::array<CLI::Option*, kEntryCount> opt{};
};

/** \brief Binds every registry knob as a long option; second name rides
 *  CLI11's comma list. */
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

/** \brief Lenient numeric read: empty/unparseable falls back to the registry
 *  default. */
template <class T> inline T num_or(const std::string& s, const char* def) {
    try {
        return static_cast<T>(std::stoll(s));
    } catch (...) { // ponytail: bad numerics silently take the default; surface when 1.2 wires logging
    }
    return static_cast<T>(std::stoll(def)); // registry defaults are well-formed
}

/** \brief Resolved strings -> typed fields. Indexes must stay in lockstep
 *  with kEntries. */
inline Config to_config(const std::array<std::string, kEntryCount>& v) {
    Config c;
    c.port = num_or<int>(v[0], kEntries[0].def);
    c.proxy = v[1];
    c.plugins_dir = v[2];
    c.fetch_timeout_ms = num_or<std::int64_t>(v[3], kEntries[3].def);
    c.fetch_profile = v[4];
    c.plugin_timeout_ops = num_or<std::int64_t>(v[5], kEntries[5].def);
    c.cache_max_mb = num_or<std::int64_t>(v[6], kEntries[6].def);
    c.image_format = v[7];
    c.cover_max_px = num_or<std::int64_t>(v[8], kEntries[8].def);
    c.db = v[9];
    c.log_file = v[10];
    c.data_dir = v[11];
    c.log_level = v[12];
    if (c.port <= 0) {
        c.port = num_or<int>(kEntries[0].def, kEntries[0].def); // design ceiling: port > 0
    }
    return c;
}

} // namespace detail

/** \brief One registry knob projected for read-only display: resolved
 *  value plus the layer that won. `source` is "default" when neither ini,
 *  env nor flag supplied a value; "ini"/"env"/"flag" otherwise (flag wins
 *  env wins ini wins default, same order as resolve). Pure re-resolve per
 *  call -- the on-disk ini may change under a long-lived daemon (D1). */
struct Describe {
    std::string section, key, full_key, doc, flag, env, def, value, source;
    Type type{};
};

inline std::vector<Describe> describe(const std::string& ini_path, const Overrides& flags,
                                      const EnvLookup& env = [](const char* n) { return std::getenv(n); }) {
    CSimpleIniA ini;
    const bool have_ini = ini.LoadFile(ini_path.c_str()) >= 0;
    std::vector<Describe> out;
    out.reserve(kEntryCount);
    for (std::size_t i = 0; i < kEntryCount; ++i) {
        const Entry& e = kEntries[i];
        Describe d;
        d.section = e.section;
        d.key = e.key;
        d.full_key = std::string(e.section) + '.' + e.key;
        d.doc = e.doc;
        d.flag = e.flag;
        d.env = e.env;
        d.def = e.def;
        d.type = e.type;
        d.value = e.def;
        d.source = "default";
        if (have_ini) {
            if (const char* iv = ini.GetValue(e.section, e.key, nullptr)) {
                d.value = iv;
                d.source = "ini";
            }
        }
        if (env) {
            if (const char* ev = env(e.env)) {
                d.value = ev;
                d.source = "env";
            }
        }
        if ((flags.opt[i] != nullptr) && flags.opt[i]->count() > 0) {
            d.value = flags.value[i];
            d.source = "flag";
        }
        if (e.type != Type::Str) { // same lenient fallback as to_config: bad numerics show the default
            try {
                std::size_t used = 0;
                std::stoll(d.value, &used);
                if (used != d.value.size())
                    throw 0;
            } catch (...) {
                d.value = e.def; // lenient fallback (same as to_config)
                d.source = "default";
            }
        }
        out.push_back(std::move(d));
    }
    return out;
}

/** \brief POST /config body coercion: JSON object -> updates map in string
 *  form. Integers up to 1e18 keep exact integer text; fractional doubles go
 *  through to_string(double); any other value type is an error. Crow-free so
 *  the branchy part is unit-testable (review I1). */
inline bool parse_updates(const std::string& body, std::map<std::string, std::string>& out,
                          std::string& err) {
    glz::generic root;
    if (glz::read_json(root, body) || !root.is_object()) {
        err = "body must be a JSON object";
        return false;
    }
    for (const auto& [key, val] : root.get_object()) {
        if (const auto* s = std::get_if<std::string>(&val.data))
            out[key] = *s;
        else if (const auto* n = std::get_if<double>(&val.data)) // glz::generic: f64 numbers
            out[key] = (*n == std::floor(*n) && std::fabs(*n) < 1e18)
                           ? std::to_string(static_cast<long long>(*n))
                           : std::to_string(*n);
        else {
            err = "value of '" + key + "' must be a string or number";
            return false;
        }
    }
    return true;
}

/** \brief Layered resolve; env lookup injectable for tests; pass {} to run
 *  with no env at all. */
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

/** \brief Minimal value escaping per SimpleIni load rules: a bare value gets
 *  trimmed, so only quote when surrounding whitespace must survive. Registry
 *  defaults are single-line; nothing else needs escaping (D4). */
inline std::string ini_literal(std::string_view v) {
    if (!v.empty() && (v.front() == ' ' || v.back() == ' '))
        return '"' + std::string(v) + '"';
    return std::string(v);
}

/** \brief D4: ini template rendered straight from the registry -- one
 *  [section] header per group, `; doc` above every key, `key = default`. Pure
 *  string on purpose: file governance lives in the caller, so dumping never
 *  touches an existing config and runs with no --config at all. */
inline std::string dump() {
    std::string out;
    std::string section;
    for (const Entry& e : kEntries) {
        if (e.section != section) { // content compare, const char* vs string
            if (!out.empty())
                out += '\n'; // blank line between groups
            section = e.section;
            out += '[';
            out += section;
            out += "]\n";
        }
        out += "; ";
        out += e.doc;
        out += '\n';
        out += e.key;
        out += " = ";
        out += ini_literal(e.def);
        out += '\n';
    }
    return out;
}

/** \brief D5: merge-save. Validation runs first -- every "section.key" must
 *  be a registry entry and numeric knobs take integer strings -- so a
 *  rejected body never touches the file. The existing ini is then edited in
 *  place by SimpleIni, which is what keeps comments and ordering; a missing
 *  file is seeded from dump() so a fresh config ships fully documented.
 *
 *  Returns keys written, -1 on rejected input (nothing written), -2 when the
 *  file cannot be written; \p err carries the reason when non-null. */
inline int save(const std::map<std::string, std::string>& updates, const std::string& ini_path,
                std::string* err = nullptr) {
    auto reject = [&](std::string why, int rc) {
        if (err != nullptr)
            *err = std::move(why);
        return rc;
    };

    // Validation pass; plan keeps entry pointers so the write pass below
    // skips the second registry scan.
    std::vector<std::pair<const Entry*, const std::string*>> plan;
    plan.reserve(updates.size());
    for (const auto& [full, value] : updates) {
        const Entry* e = nullptr;
        for (const Entry& c : kEntries)
            if (full == std::string(c.section) + '.' + c.key) {
                e = &c;
                break;
            }
        if (e == nullptr)
            return reject("unknown config key: " + full, -1);
        if (e->type != Type::Str) {
            try {
                std::size_t used = 0;
                std::stoll(value, &used);
                if (used != value.size())
                    return reject("value of " + full + " is not an integer", -1);
            } catch (...) { // empty / non-numeric: same 400-class reject
                return reject("value of " + full + " is not an integer", -1);
            }
        }
        plan.push_back({e, &value});
    }
    if (plan.empty())
        return 0; // nothing to merge: no load, no rewrite

    CSimpleIniA ini;
    if (std::filesystem::exists(ini_path)) {
        if (ini.LoadFile(ini_path.c_str()) < 0)
            return reject("cannot parse " + ini_path, -1); // corrupt: never clobber
    } else {
        ini.LoadData(dump()); // D5: absent file starts from the documented template
    }

    for (const auto& [e, value] : plan)
        ini.SetValue(e->section, e->key, value->c_str());

    if (ini.SaveFile(ini_path.c_str(), false) < 0) // no BOM: plain template shape
        return reject("cannot write " + ini_path, -2);
    return static_cast<int>(plan.size());
}

} // namespace config
