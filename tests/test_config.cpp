#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <CLI/CLI.hpp>

#include <algorithm>

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
    std::string path =
        (std::filesystem::temp_directory_path() / ("cppisekai_cfg_test_" + std::to_string(++tmp_seq) + ".ini"))
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
    std::vector<std::string> argv;
    for (const char* a : args) argv.emplace_back(a);
    std::reverse(argv.begin(), argv.end()); // CLI11 vector<string> overload consumes reversed
    app.parse(argv);
    return o;
}

} // namespace

TEST_CASE("config: layered resolve default < ini < env < flag") {
    Overrides none; // no CLI parse: flag layer inert, opt[] all null

    SUBCASE("defaults when no ini and no env") {
        Config c = resolve(none, "no-such-cfg.ini", fake_env({}));
        CHECK(c.port == 8090);
        CHECK(c.proxy == "");
        CHECK(c.plugins_dir == "data/plugins");
        CHECK(c.fetch_timeout_ms == 30000);
        CHECK(c.fetch_profile == "chrome116");
        CHECK(c.plugin_timeout_ops == 5000000);
        CHECK(c.cache_max_mb == 2048);
        CHECK(c.image_format == "webp");
        CHECK(c.cover_max_px == 800);
        CHECK(c.db == "data/library.db");
        CHECK(c.log_file == "data/cppisekai.log");
        CHECK(c.data_dir == "data");
    }

    SUBCASE("ini beats default; env beats ini and default") {
        auto ini = tmp_ini("[server]\nport = 1234\n[cache]\ncache_max_mb = 64\n");
        Config a = resolve(none, ini, fake_env({}));
        CHECK(a.port == 1234);
        CHECK(a.cache_max_mb == 64);
        CHECK(a.fetch_profile == "chrome116"); // untouched knob keeps default

        CHECK(resolve(none, ini, fake_env({{"CPPISEKAI_PORT", "2222"}})).port == 2222);
        CHECK(resolve(none, "no-such-cfg.ini", fake_env({{"CPPISEKAI_FETCH_PROFILE", "chrome120"}})).fetch_profile ==
              "chrome120");
        CHECK(resolve(none, ini, fake_env({{"CPPISEKAI_PORT", "2222"}})).cache_max_mb == 64);
    }

    SUBCASE("explicit flag beats env; unset flag keeps ini") {
        auto ini = tmp_ini("[server]\nport = 1234\n[cache]\ncache_max_mb = 64\n");
        Overrides flagged = parse_args({"--port", "9000"});
        Config c = resolve(flagged, ini, fake_env({{"CPPISEKAI_PORT", "2222"}}));
        CHECK(c.port == 9000);
        CHECK(c.cache_max_mb == 64); // --cache-max-mb not passed: ini value survives
    }

    SUBCASE("alias --server-port binds the same knob") {
        CHECK(resolve(parse_args({"--server-port", "7777"}), "no-such-cfg.ini", fake_env({})).port == 7777);
    }

    SUBCASE("update knobs: defaults, bounds and round-trip") {
        Config d = resolve(none, "no-such-cfg.ini", fake_env({}));
        CHECK(d.update_interval_days == 1);
        CHECK(d.update_per_plugin_limit == 10);
        CHECK(d.update_request_delay_ms == 1000);
        auto ini = tmp_ini("[update]\ninterval_days = 2\nper_plugin_limit = 5\nrequest_delay_ms = 0\n");
        Config a = resolve(none, ini, fake_env({}));
        CHECK(a.update_interval_days == 2);
        CHECK(a.update_per_plugin_limit == 5);
        CHECK(a.update_request_delay_ms == 0);
        auto bad = tmp_ini("[update]\ninterval_days = 0\nper_plugin_limit = 0\nrequest_delay_ms = -1\n");
        Config b = resolve(none, bad, fake_env({})); // boot: out-of-bounds falls back, never applied
        CHECK(b.update_interval_days == 1);
        CHECK(b.update_per_plugin_limit == 10);
        CHECK(b.update_request_delay_ms == 1000);
        CHECK(resolve(none, "no-such-cfg.ini", fake_env({{"CPPISEKAI_UPDATE_INTERVAL_DAYS", "3"},
                                                         {"CPPISEKAI_UPDATE_PER_PLUGIN_LIMIT", "7"},
                                                         {"CPPISEKAI_UPDATE_REQUEST_DELAY_MS", "2500"}}))
                  .update_request_delay_ms == 2500);
    }

    SUBCASE("port <= 0 falls back to default (validation ceiling)") {
        auto ini = tmp_ini("[server]\nport = 0\n");
        CHECK(resolve(none, ini, fake_env({})).port == 8090);
    }

    SUBCASE("cache knobs round-trip env and flag") {
        auto ini = tmp_ini("[cache]\nimage_format = avif\ncover_max_px = 400\n");
        Config a = resolve(none, ini, fake_env({}));
        CHECK(a.image_format == "avif");
        CHECK(a.cover_max_px == 400);

        Config b = resolve(none, "no-such-cfg.ini",
                           fake_env({{"CPPISEKAI_IMAGE_FORMAT", "jxl"}, {"CPPISEKAI_COVER_MAX_PX", "1200"}}));
        CHECK(b.image_format == "jxl");
        CHECK(b.cover_max_px == 1200);

        Overrides flagged = parse_args({"--image-format", "none", "--cover-max-px", "600"});
        Config c = resolve(flagged, ini, fake_env({{"CPPISEKAI_IMAGE_FORMAT", "jxl"}}));
        CHECK(c.image_format == "none"); // explicit flag beats env and ini
        CHECK(c.cover_max_px == 600);
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
    CHECK(headers == 6); // one header per group: server plugin cache paths log update

    // And the template is honest ini: SimpleIni reads every default back.
    CSimpleIniA ini;
    REQUIRE(ini.LoadData(text) >= 0);
    for (const Entry& e : kEntries)
        CHECK(std::string(ini.GetValue(e.section, e.key, "\x01missing")) == std::string(e.def));
}

// D5: comment-safe merge-save. `read` is the byte-honest view the asserts
// below reason about -- SimpleIni re-emits `key = value` with spaces.
TEST_CASE("config: parse_updates coerces POST /config body (review I1)") {
    std::map<std::string, std::string> out;
    std::string err;

    auto ok = [&](const char* body) {
        out.clear();
        err.clear();
        return parse_updates(body, out, err);
    };

    SUBCASE("object with string and fractional-free number") {
        REQUIRE(ok(R"({"server.port": 8091, "server.proxy": "http://p:1"})"));
        CHECK(out.count("server.port") == 1);
        CHECK(out.at("server.port") == "8091");
        CHECK(out.at("server.proxy") == "http://p:1");
        CHECK(err.empty());
    }

    SUBCASE("integer below 2^53 stays exact (no float rounding)") {
        REQUIRE(ok(R"({"plugin.plugin_timeout_ops": 1234567890123456})"));
        // glz::generic holds numbers as f64, so only values <= 2^53 round-trip
        // exactly; the integer branch still renders them without decimals.
        CHECK(out.at("plugin.plugin_timeout_ops") == "1234567890123456");
    }

    SUBCASE("integer above 2^53 keeps the f64 rounding glz gives us (ponytail)") {
        // Known ceiling: glz::generic is f64-only, values past 2^53 lose the
        // exact-int guarantee in the coerced string. Upgrade when glaze grows
        // an int64 variant path.
        REQUIRE(ok(R"({"plugin.plugin_timeout_ops": 12345678901234567})"));
        CHECK(out.at("plugin.plugin_timeout_ops") == "12345678901234568");
    }

    SUBCASE("fractional double goes through to_string(double)") {
        REQUIRE(ok(R"({"server.proxy": 1.5})"));
        CHECK(out.at("server.proxy") == "1.500000");
    }

    SUBCASE("huge beyond 1e18 falls back to double text") {
        REQUIRE(ok(R"({"plugin.plugin_timeout_ops": 1e19})"));
        CHECK(out.at("plugin.plugin_timeout_ops") == "10000000000000000000.000000");
    }

    SUBCASE("non-scalar value rejected with key named") {
        CHECK_FALSE(ok(R"({"server.port": {"a":1}})"));
        CHECK(err == "value of 'server.port' must be a string or number");
    }

    SUBCASE("non-object body rejected") {
        CHECK_FALSE(ok("[1,2,3]"));
        CHECK(err == "body must be a JSON object");
    }

    SUBCASE("malformed JSON rejected") {
        CHECK_FALSE(ok("{not json"));
        CHECK(err == "body must be a JSON object");
    }
}

TEST_CASE("config: save merges comment-safe (D5)") {
    std::string err;
    auto read = [](const std::string& p) {
        std::ifstream f{p};
        return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    };

    SUBCASE("round trip: comments, ordering and untouched keys survive") {
        const auto path = tmp_ini("[server]\n; TCP listen port\nport = 1234\n; keep me\nproxy = http://p:1\n");
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

    SUBCASE("out-of-bounds numeric rejected, file untouched") {
        const auto path = tmp_ini("[update]\ninterval_days = 1\n");
        const auto before = read(path);
        CHECK(save({{"update.interval_days", "0"}}, path, &err) == -1);
        CHECK(err == "value of update.interval_days is below the minimum of 1");
        CHECK(save({{"update.per_plugin_limit", "0"}}, path, &err) == -1);
        CHECK(save({{"update.request_delay_ms", "-1"}}, path, &err) == -1);
        CHECK(save({{"update.interval_days", "2"}, {"update.per_plugin_limit", "5"}, {"update.request_delay_ms", "2500"}}, path, &err) == 3);
        CHECK(save({{"update.request_delay_ms", "0"}}, path, &err) == 1);
        CHECK(read(path).find("interval_days = 2") != std::string::npos);
    }

    SUBCASE("missing file: created from the dump template, resolve() reads it back") {
        const auto path =
            (std::filesystem::temp_directory_path() / ("cppisekai_cfg_fresh_" + std::to_string(++tmp_seq) + ".ini"))
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

TEST_CASE("config: describe() projects registry with layer provenance (D1/D3)") {
    Overrides none; // flag layer inert
    auto ini = tmp_ini("[server]\nport = 1234\n[cache]\ncache_max_mb = bogus\n");

    const auto d = describe(ini, none, fake_env({{"CPPISEKAI_PORT", "9999"}}));
    REQUIRE(d.size() == kEntryCount);
    const auto by_full = [&](const char* f) -> const Describe& {
        for (const auto& x : d)
            if (x.full_key == f) return x;
        throw 1;
    };

    CHECK(by_full("server.port").value == "9999"); // env beats ini
    CHECK(by_full("server.port").source == "env");
    CHECK(by_full("cache.cache_max_mb").value == kEntries[6].def); // bad numeric -> default fallback
    CHECK(by_full("cache.cache_max_mb").source == "default");
    CHECK(by_full("cache.image_format").source == "default"); // untouched by ini
    CHECK(by_full("server.proxy").source == "default");

    // resolver does not write the file
    auto read = [](const std::string& p) {
        std::ifstream f{p};
        return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    };
    const auto before = read(ini);
    describe(ini, none, fake_env({}));
    CHECK(read(ini) == before);

    // flag beats env beats ini
    auto flagged = parse_args({"--port", "7777"});
    const auto d2 = describe(ini, flagged, fake_env({{"CPPISEKAI_PORT", "9999"}}));
    for (const auto& x : d2)
        if (x.full_key == "server.port") {
            CHECK(x.value == "7777");
            CHECK(x.source == "flag");
        }
}
