#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <CLI/CLI.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "config/config.hpp"

using namespace config;

namespace {

int tmp_seq = 0;

std::string tmp_ini(const std::string& body) {
    std::string path = (std::filesystem::temp_directory_path() /
                        ("cppisekai_cfg_test_" + std::to_string(++tmp_seq) + ".ini"))
                           .string();
    std::ofstream{path} << body;
    return path;
}

// Hermetic env layer: a closed map, never the process environment.
EnvLookup fake_env(std::map<std::string, std::string> m) {
    return [m = std::move(m)](const char* n) {
        auto it = m.find(n);
        return it == m.end() ? nullptr : it->second.c_str();
    };
}

Overrides parse_args(std::initializer_list<const char*> args) {
    Overrides o;
    CLI::App app{"test"};
    build_cli(app, o);
    std::vector<const char*> argv{args};
    app.parse(static_cast<int>(argv.size()), argv.data());
    return o;
}

}  // namespace

TEST_CASE("config: layered resolve default < ini < env < flag") {
    Overrides none;  // no CLI parse: flag layer inert, opt[] all null

    SUBCASE("defaults when no ini and no env") {
        Config c = resolve(none, "no-such-cfg.ini", fake_env({}));
        CHECK(c.port == 8090);
        CHECK(c.proxy == "");
        CHECK(c.plugins_dir == "data/plugins");
        CHECK(c.fetch_timeout_ms == 30000);
        CHECK(c.fetch_profile == "chrome116");
        CHECK(c.plugin_timeout_ops == 5000000);
        CHECK(c.cache_max_mb == 2048);
        CHECK(c.db == "data/library.db");
        CHECK(c.log_file == "data/cppisekai.log");
        CHECK(c.data_dir == "data");
    }

    SUBCASE("ini beats default; env beats ini and default") {
        auto ini = tmp_ini("[server]\nport = 1234\n[cache]\ncache_max_mb = 64\n");
        Config a = resolve(none, ini, fake_env({}));
        CHECK(a.port == 1234);
        CHECK(a.cache_max_mb == 64);
        CHECK(a.fetch_profile == "chrome116");  // untouched knob keeps default

        CHECK(resolve(none, ini, fake_env({{"CPPISEKAI_PORT", "2222"}})).port == 2222);
        CHECK(resolve(none, "no-such-cfg.ini",
                      fake_env({{"CPPISEKAI_FETCH_PROFILE", "chrome120"}}))
                      .fetch_profile == "chrome120");
        CHECK(resolve(none, ini, fake_env({{"CPPISEKAI_PORT", "2222"}})).cache_max_mb == 64);
    }

    SUBCASE("explicit flag beats env; unset flag keeps ini") {
        auto ini = tmp_ini("[server]\nport = 1234\n[cache]\ncache_max_mb = 64\n");
        Overrides flagged = parse_args({"prog", "--port", "9000"});
        Config c = resolve(flagged, ini, fake_env({{"CPPISEKAI_PORT", "2222"}}));
        CHECK(c.port == 9000);
        CHECK(c.cache_max_mb == 64);  // --cache-max-mb not passed: ini value survives
    }

    SUBCASE("alias --server-port binds the same knob") {
        CHECK(resolve(parse_args({"prog", "--server-port", "7777"}), "no-such-cfg.ini",
                      fake_env({}))
                      .port == 7777);
    }

    SUBCASE("port <= 0 falls back to default (validation ceiling)") {
        auto ini = tmp_ini("[server]\nport = 0\n");
        CHECK(resolve(none, ini, fake_env({})).port == 8090);
    }

    SUBCASE("garbage numeric falls back to default") {
        auto ini = tmp_ini("[cache]\ncache_max_mb = lots\n");
        CHECK(resolve(none, ini, fake_env({})).cache_max_mb == 2048);
    }
}

TEST_CASE("config: dump projects registry as commented ini template (D4)") {
    const std::string text = dump();
    REQUIRE(!text.empty());

    // key -> {section header, doc comment, rendered key = value}
    std::map<std::string, std::array<std::string, 3>> row;
    std::string section;
    std::string prev;
    std::istringstream in(text);
    int headers = 0;
    for (std::string line; std::getline(in, line); prev = line) {
        if (line.starts_with('[')) {
            section = line;
            ++headers;
        } else if (!line.empty() && !line.starts_with("; ")) {
            const auto eq = line.find(" = ");
            REQUIRE(eq != std::string::npos);
            row[line.substr(0, eq)] = {section, prev, line};
        }
    }

    // Every registry knob present exactly once: right section, doc above,
    // default as value -- generated, so new knobs are covered for free.
    for (const Entry& e : kEntries) {
        const std::string key = e.key;
        REQUIRE(row.count(key) == 1);
        CHECK(row[key][0] == "[" + std::string(e.section) + "]");
        CHECK(row[key][1] == "; " + std::string(e.doc));
        CHECK(row[key][2] == key + " = " + std::string(e.def));
    }
    CHECK(headers == 4); // one header per group: server plugin cache paths

    // And the template is honest ini: SimpleIni reads every default back.
    CSimpleIniA ini;
    REQUIRE(ini.LoadData(text) >= 0);
    for (const Entry& e : kEntries)
        CHECK(std::string(ini.GetValue(e.section, e.key, "\x01missing")) ==
              std::string(e.def));
}
