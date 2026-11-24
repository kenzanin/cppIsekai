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


struct Config {
    int port;
    std::string proxy;
    std::string plugins_dir;
    std::int64_t fetch_timeout_ms;
    std::string fetch_profile;
    std::int64_t plugin_timeout_ops;
    std::int64_t cache_max_mb;
    std::string image_format;  // [cache] image_format: webp|avif|jxl|none
    std::int64_t cover_max_px; // [cache] cover_max_px: cover resize cap
    std::string db;
    std::string log_file;
    std::string data_dir;
    std::string log_level;         // [log] level: trace|debug|info|warning|error
    std::string templates_dir;     // [paths] templates_dir: mustache template root
    std::string enrichment_dir;    // [paths] enrichment_dir: metadata-only enrichment plugins (NOT scraper plugins)
    int update_interval_days{};    // [update] interval_days: days between auto-update runs
    int update_per_plugin_limit{}; // [update] per_plugin_limit: max chapter-list refreshes per plugin per run
    int update_request_delay_ms{}; // [update] request_delay_ms: pause between auto-update requests
    std::string chrome_command; // [chrome] command: browser binary/wrapper; empty = auto-discover (ini-only)
    std::string chrome_args;    // [chrome] args: extra flags, space-split (ini-only)
    std::string chrome_headless; // [chrome] headless: "1"/"0", default headed-off (ini-only)
    int backup_interval_h{}; // [backup] interval_h: hours between VACUUM+snapshot runs, 0 = off
    int backup_keep{};       // [backup] keep: snapshot files kept under <db dir>/backups
    std::string tray_enabled;      // [tray] enabled: "1" shows the tray, "0" leaves it off
    std::string tray_icon;         // [tray] icon: icon file path
    std::string tray_title;        // [tray] title: StatusNotifier app name
    std::string tray_label;        // [tray] label: status row, {port} = listen port
    std::string tray_open_label;    // [tray] open_label: open-reader entry
    std::string tray_restart_label; // [tray] restart_label: restart entry
    std::string tray_stop_label;    // [tray] stop_label: stop entry
    std::string assets_path;        // [assets] path: zip archive or folder root, empty = repo layout
};

// Per-knob CLI state. Explicit = opt->count() > 0 (D7), never value comparison;
// unset flags leave value untouched so ini/env survive.
struct Overrides {
    std::array<std::string, kEntryCount> value; // last argument per knob, meaningful iff explicit
    std::array<CLI::Option*, kEntryCount> opt{};
};

} // namespace config
