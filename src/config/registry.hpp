#pragma once

/** \file registry.hpp
 *  \brief The single source of config truth: every knob, its layers, its
 *  default. Registry table is normative:
 *  openspec/changes/cli-config/design.md (D1, D7). */

#include <array>
#include <cstddef>

namespace config {

enum class Type { Int, Int64, Str };

struct Entry {
    const char* section;
    const char* key;
    const char* flag;  // primary long name, no dashes
    const char* alias; // optional second long name, "" if none
    const char* env;   // CPPISEKAI_* name
    const char* def;
    const char* doc;
    Type type;
};

// Order is the projection order consumed by config.hpp -- do not reorder.
inline constexpr std::array<Entry, 15> kEntries{
    {
        // section      key                   flag                 alias          env def doc
        // type
        {.section = "server",
         .key = "port",
         .flag = "port",
         .alias = "server-port",
         .env = "CPPISEKAI_PORT",
         .def = "8090",
         .doc = "TCP listen port",
         .type = Type::Int},
        {.section = "server",
         .key = "proxy",
         .flag = "proxy",
         .alias = "",
         .env = "CPPISEKAI_PROXY",
         .def = "",
         .doc = "Outbound HTTP(S) proxy URL",
         .type = Type::Str},
        {.section = "plugin",
         .key = "plugins",
         .flag = "plugins-dir",
         .alias = "",
         .env = "CPPISEKAI_PLUGINS_DIR",
         .def = "data/plugins",
         .doc = "Plugin directory",
         .type = Type::Str},
        {.section = "plugin",
         .key = "fetch_timeout_ms",
         .flag = "fetch-timeout-ms",
         .alias = "",
         .env = "CPPISEKAI_FETCH_TIMEOUT_MS",
         .def = "30000",
         .doc = "HTTP fetch timeout (ms)",
         .type = Type::Int64},
        {.section = "plugin",
         .key = "fetch_profile",
         .flag = "fetch-profile",
         .alias = "",
         .env = "CPPISEKAI_FETCH_PROFILE",
         .def = "chrome116",
         .doc = "curl-impersonate target; dropdown list from [plugin] fetch_profiles",
         .type = Type::Str},
        {.section = "plugin",
         .key = "plugin_timeout_ops",
         .flag = "plugin-timeout-ops",
         .alias = "",
         .env = "CPPISEKAI_PLUGIN_TIMEOUT_OPS",
         .def = "5000000",
         .doc = "Wasm instruction budget per call",
         .type = Type::Int64},
        {.section = "cache",
         .key = "cache_max_mb",
         .flag = "cache-max-mb",
         .alias = "",
         .env = "CPPISEKAI_CACHE_MAX_MB",
         .def = "2048",
         .doc = "Image cache cap (MiB); 2048 MiB = 2 GiB, 4096 MiB = 4 GiB",
         .type = Type::Int64},
        {.section = "cache",
         .key = "image_format",
         .flag = "image-format",
         .alias = "",
         .env = "CPPISEKAI_IMAGE_FORMAT",
         .def = "webp",
         .doc = "Stored image format (webp|avif|jxl|none)",
         .type = Type::Str},
        {.section = "cache",
         .key = "cover_max_px",
         .flag = "cover-max-px",
         .alias = "",
         .env = "CPPISEKAI_COVER_MAX_PX",
         .def = "800",
         .doc = "Cover longest-side cap (px)",
         .type = Type::Int64},
        {.section = "paths",
         .key = "library",
         .flag = "db",
         .alias = "",
         .env = "CPPISEKAI_DB",
         .def = "data/library.db",
         .doc = "SQLite library path",
         .type = Type::Str},
        {.section = "paths",
         .key = "log_file",
         .flag = "log-file",
         .alias = "",
         .env = "CPPISEKAI_LOG_FILE",
         .def = "data/cppisekai.log",
         .doc = "Log file path",
         .type = Type::Str},
        {.section = "paths",
         .key = "data_dir",
         .flag = "data-dir",
         .alias = "",
         .env = "CPPISEKAI_DATA_DIR",
         .def = "data",
         .doc = "Data root",
         .type = Type::Str},
        {.section = "paths",
         .key = "templates_dir",
         .flag = "templates-dir",
         .alias = "",
         .env = "CPPISEKAI_TEMPLATES_DIR",
         .def = "templates",
         .doc = "Template directory (mustache partials read from disk per request)",
         .type = Type::Str},
        {.section = "paths",
         .key = "enrichment",
         .flag = "enrichment-dir",
         .alias = "",
         .env = "CPPISEKAI_ENRICHMENT_DIR",
         .def = "data/enrichment",
         .doc = "Enrichment plugin directory (metadata-only sources; NOT scraper plugins)",
         .type = Type::Str},
        {.section = "log",
         .key = "level",
         .flag = "log-level",
         .alias = "",
         .env = "CPPISEKAI_LOG_LEVEL",
         .def = "info",
         .doc = "Log level (trace|debug|info|warning|error)",
         .type = Type::Str},
    },
};

inline constexpr std::size_t kEntryCount = kEntries.size();

} // namespace config
