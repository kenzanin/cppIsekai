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

// D5: comment-safe merge-save. `read` is the byte-honest view the asserts
// below reason about -- SimpleIni re-emits `key = value` with spaces.
TEST_CASE("config: save merges comment-safe (D5)") {
    std::string err;
    auto read = [](const std::string& p) {
        std::ifstream f{p};
        return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    };

    SUBCASE("round trip: comments, ordering and untouched keys survive") {
        const auto path =
            tmp_ini("[server]\n; TCP listen port\nport = 1234\n; keep me\nproxy = http://p:1\n");
        REQUIRE(save({{"server.port", "8091"}}, path, &err) == 1);
        CHECK(err.empty());
        const auto text = read(path);
        CHECK(text.find("; TCP listen port") != std::string::npos);
        CHECK(text.find("; keep me") != std::string::npos);
        const auto port_at = text.find("port = 8091");
        const auto proxy_at = text.find("proxy = http://p:1");
        REQUIRE(port_at != std::string::npos);
        REQUIRE(proxy_at != std::string::npos);
        CHECK(port_at < proxy_at); // load order preserved
    }

    SUBCASE("unknown key rejected before any write") {
        const auto path = tmp_ini("[server]\nport = 1\n");
        const auto before = read(path);
        CHECK(save({{"nope.side", "x"}}, path, &err) == -1);
        CHECK(err == "unknown config key: nope.side");
        CHECK(read(path) == before); // validation-first: file untouched
    }

    SUBCASE("type mismatch rejected: numeric knob takes an integer string") {
        const auto path = tmp_ini("[server]\n; c\nport = 1\n");
        const auto before = read(path);
        CHECK(save({{"server.port", "eighty"}}, path, &err) == -1);
        CHECK(err == "value of server.port is not an integer");
        CHECK(read(path) == before);
    }

    SUBCASE("unregistered key beside a valid one is still a no-op write") {
        const auto path = tmp_ini("[paths]\nlog_file = a.log\n");
        const auto before = read(path);
        CHECK(save({{"paths.data_dir", "d"}, {"made.up", "1"}}, path, &err) == -1);
        CHECK(read(path) == before);
    }

    SUBCASE("missing file: created from the dump template, resolve() reads it back") {
        const auto path = (std::filesystem::temp_directory_path() /
                           ("cppisekai_cfg_fresh_" + std::to_string(++tmp_seq) + ".ini"))
                              .string();
        std::filesystem::remove(path); // seq restarts per run: clear prior leftovers
        REQUIRE(save({{"server.port", "1"}}, path, &err) == 1);
        CHECK(err.empty());
        const auto text = read(path);
        for (const Entry& e : kEntries) // every knob, doc comment attached
            CHECK_MESSAGE(text.find(e.key) != std::string::npos, e.key);
        CHECK(text.find("; TCP listen port") != std::string::npos);
        CHECK(resolve(Overrides{}, path, fake_env({})).port == 1);
    }
}
