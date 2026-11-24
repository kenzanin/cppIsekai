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

/** \brief Binds every registry knob as a long option; second name rides
 *  CLI11's comma list. */
inline void build_cli(CLI::App& app, Overrides& o) {
    for (std::size_t i = 0; i < kEntryCount; ++i) {
        const Entry& e = kEntries[i];
        if (e.flag == nullptr || *e.flag == 0) {
            continue; // ini-only knob (e.g. [chrome]): no CLI surface, opt stays null
        }
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
    c.templates_dir = v[12];
    c.enrichment_dir = v[13];
    c.log_level = v[14];
    c.update_interval_days = num_or<int>(v[15], kEntries[15].def);
    if (c.update_interval_days < 1) {
        c.update_interval_days = num_or<int>(kEntries[15].def, kEntries[15].def); // min 1 (task 1.2)
    }
    c.update_per_plugin_limit = num_or<int>(v[16], kEntries[16].def);
    if (c.update_per_plugin_limit < 1) {
        c.update_per_plugin_limit = num_or<int>(kEntries[16].def, kEntries[16].def); // min 1
    }
    c.update_request_delay_ms = num_or<int>(v[17], kEntries[17].def);
    if (c.update_request_delay_ms < 0) {
        c.update_request_delay_ms = num_or<int>(kEntries[17].def, kEntries[17].def); // min 0
    }
    c.chrome_command = v[18];
    c.chrome_args = v[19];
    c.chrome_headless = v[20];
    c.backup_interval_h = num_or<int>(v[21], kEntries[21].def);
    if (c.backup_interval_h < 0) {
        c.backup_interval_h = 0;
    }
    c.backup_keep = num_or<int>(v[22], kEntries[22].def);
    if (c.backup_keep < 1) {
        c.backup_keep = num_or<int>(kEntries[22].def, kEntries[22].def); // min 1
    }
    c.tray_enabled = v[23];
    c.tray_icon = v[24];
    c.tray_title = v[25];
    c.tray_label = v[26];
    c.tray_open_label = v[27];
    c.tray_restart_label = v[28];
    c.tray_stop_label = v[29];
    c.assets_path = v[30];
    if (c.port <= 0) {
        c.port = num_or<int>(kEntries[0].def, kEntries[0].def); // design ceiling: port > 0
    }
    return c;
}

} // namespace detail

} // namespace config
