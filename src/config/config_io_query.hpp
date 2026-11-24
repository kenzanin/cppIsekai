#pragma once

/** \file config.hpp
 *  \brief Config core: layered resolve (default < ini < env < explicit flag)
 *  over the registry. All config knowledge lives in src/config/ (D1);
 *  header-only, no .cpp. */

#include "registry.hpp"

#include <CLI/CLI.hpp>
#include "glaze/glaze.hpp"

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

/** \brief One registry knob projected for read-only display: resolved
 *  value plus the layer that won. `source` is "default" when neither ini,
 *  env nor flag supplied a value; "ini"/"env"/"flag" otherwise (flag wins
 *  env wins ini wins default, same order as resolve). Pure re-resolve per
 *  call -- the on-disk ini may change under a long-lived daemon (D1). */
struct Describe {
    std::string section, key, full_key, doc, flag, env, def, value, source;
    Type type{};
    bool readonly{};
};

inline std::vector<Describe> describe(
    const std::string& ini_path, const Overrides& flags,
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
        d.readonly = e.readonly;
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
                if (used != d.value.size()) {
                    throw 0;
                }
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
inline bool parse_updates(const std::string& body, std::map<std::string, std::string>& out, std::string& err) {
    glz::generic root;
    if (glz::read_json(root, body) || !root.is_object()) {
        err = "body must be a JSON object";
        return false;
    }
    for (const auto& [key, val] : root.get_object()) {
        if (const auto* s = std::get_if<std::string>(&val.data)) {
            {
                out[key] = *s;
            }
        } else if (const auto* n = std::get_if<double>(&val.data)) {
            { // glz::generic: f64 numbers
                out[key] = (*n == std::floor(*n) && std::fabs(*n) < 1e18) ? std::to_string(static_cast<long long>(*n))
                                                                          : std::to_string(*n);
            }
        } else {
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

} // namespace config
