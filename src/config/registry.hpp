// The single source of config truth: every knob, its layers, its default.
// Registry table is normative: openspec/changes/cli-config/design.md (D1, D7).
#pragma once

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
inline constexpr std::array<Entry, 10> kEntries{{
    // section      key                   flag                 alias          env                               def doc
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
     .doc = "curl-impersonate target",
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
     .doc = "Image cache cap (MiB)",
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
}};

inline constexpr std::size_t kEntryCount = kEntries.size();

} // namespace config
