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

inline std::string ini_literal(std::string_view v) {
    if (!v.empty() && (v.front() == ' ' || v.back() == ' ')) {
        return '"' + std::string(v) + '"';
    }
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
            if (!out.empty()) {
                out += '\n'; // blank line between groups
            }
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
    auto reject = [&](const std::string& why, int rc) {
        if (err != nullptr) {
            *err = why;
        }
        return rc;
    };

    // Validation pass; plan keeps entry pointers so the write pass below
    // skips the second registry scan.
    std::vector<std::pair<const Entry*, const std::string*>> plan;
    plan.reserve(updates.size());
    for (const auto& [full, value] : updates) {
        const Entry* e = nullptr;
        for (const Entry& c : kEntries) {
            if (full == std::string(c.section) + '.' + c.key) {
                e = &c;
                break;
            }
        }
        if (e == nullptr) {
            return reject("unknown config key: " + full, -1);
        }
        if (e->type != Type::Str) {
            long long parsed = 0;
            try {
                std::size_t used = 0;
                parsed = std::stoll(value, &used);
                if (used != value.size()) {
                    return reject("value of " + full + " is not an integer", -1);
                }
            } catch (...) { // empty / non-numeric: same 400-class reject
                return reject("value of " + full + " is not an integer", -1);
            }
            if (parsed < e->min_value) {
                return reject("value of " + full + " is below the minimum of " + std::to_string(e->min_value), -1);
            }
        }
        plan.emplace_back(e, &value);
    }
    if (plan.empty()) {
        return 0; // nothing to merge: no load, no rewrite
    }

    CSimpleIniA ini;
    if (std::filesystem::exists(ini_path)) {
        if (ini.LoadFile(ini_path.c_str()) < 0) {
            return reject("cannot parse " + ini_path, -1); // corrupt: never clobber
        }
    } else {
        ini.LoadData(dump()); // D5: absent file starts from the documented template
    }

    for (const auto& [e, value] : plan) {
        ini.SetValue(e->section, e->key, value->c_str());
    }

    if (ini.SaveFile(ini_path.c_str(), false) < 0) { // no BOM: plain template shape
        return reject("cannot write " + ini_path, -2);
    }
    return static_cast<int>(plan.size());
}


} // namespace config
