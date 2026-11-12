#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "log/log.hpp"
#include "plugin/http_client.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
plugin::Config test_conf() {
    plugin::Config c;
    c.dir = TEST_PLUGIN_FIXTURES;
    c.instruction_budget = 200000; // small: loop fixture must trip fast
    return c;
}

// Same queue-sink wiring as main.cpp so plugin log lines are observable.
struct LogSetup {
    LogSetup() {
        spdlog::details::registry::instance().drop("test");
        auto base = spdlog::stdout_color_mt("test");
        base->sinks().push_back(std::make_shared<QueueSink>());
        spdlog::set_default_logger(base);
    }
};
} // namespace

TEST_CASE("plugin loader") {
    LogSetup ls;
    Store store("test_plugin.db");
    plugin::Host host(test_conf());

    SUBCASE("valid manifests load, broken ones skip with warning") {
        const int n = host.load_all(store);
        CHECK(n >= 5); // good, dup, os_user, loop, fetcher (dup pair keeps one)
        CHECK(host.call("good", "ping", "{}").find("\"ok\":true") != std::string::npos);
        CHECK(host.call("good", "ping", "{}").find("no plugin") == std::string::npos);
    }

    SUBCASE("duplicate id: first wins") {
        host.load_all(store);
        const std::string res = host.call("dup", "ping", "{}");
        CHECK(res.find("first") != std::string::npos);
    }

    SUBCASE("future api rejected at load") {
        host.load_all(store);
        CHECK(host.call("future", "ping", "{}").find("no plugin") != std::string::npos);
    }

    SUBCASE("broken plugins absent from store upserts") {
        host.load_all(store);
        bool found = false;
        for (const auto& p : store.plugins())
            if (p.plugin_id == "good") {
                found = true;
                CHECK(p.name == "Good Plugin");
            }
        CHECK(found);
    }
}

TEST_CASE("ABI envelope dispatch") {
    LogSetup ls;
    Store store("test_plugin_abi.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    SUBCASE("ok round-trip carries params") {
        const std::string res = host.call("good", "ping", "{\"echo\":\"hi\"}");
        CHECK(res.find("\"pong\":true") != std::string::npos);
        CHECK(res.find("\"hi\"") != std::string::npos);
    }

    SUBCASE("error round-trip") {
        const std::string res = host.call("good", "fail", "{}");
        CHECK(res.find("\"ok\":false") != std::string::npos);
        CHECK(res.find("boom") != std::string::npos);
    }

    SUBCASE("malformed return treated as failure") {
        CHECK(host.call("good", "weird", "{}").find("\"ok\":false") != std::string::npos);
        CHECK(host.call("good", "no_ok", "{}").find("\"ok\":false") != std::string::npos);
    }

    SUBCASE("os sandbox closed: os.execute raises plugin error") {
        const std::string res = host.call("os_user", "ping", "{}");
        CHECK(res.find("\"ok\":false") != std::string::npos);
    }

    SUBCASE("crypto vrf_sign via plugin returns golden signature") {
        const std::string res = host.call("crypto", "vrf", "{\"path\":\"/titles\",\"params\":{}}");
        CHECK(res.find("\"ok\":true") != std::string::npos);
        CHECK(res.find("8sK3xtqdFQ") != std::string::npos);
    }

    SUBCASE("infinite loop times out with detectable error") {
        const std::string res = host.call("loop", "ping", "{}");
        CHECK(res.find("\"ok\":false") != std::string::npos);
        CHECK(res.find("timeout") != std::string::npos);
    }
}

TEST_CASE("host tools: log + now") {
    LogSetup ls;
    Store store("test_plugin_tools.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::time_t before = std::time(nullptr);
    const std::string res = host.call("good", "log", "{}");
    const std::time_t after = std::time(nullptr);

    LogBuffer drain("test_plugin_tools.log");
    drain.drain();
    bool seen = false;
    for (const auto& line : drain.snapshot())
        if (line.find("[plugin:good]") != std::string::npos && line.find("good plugin says hi") != std::string::npos)
            seen = true;
    CHECK(seen);

    // now() reached the envelope as integer unix seconds within 2s.
    const auto pos = res.find("\"t\":");
    REQUIRE(pos != std::string::npos);
    const long long t = std::atoll(res.c_str() + pos + 4);
    CHECK(t >= static_cast<long long>(before) - 2);
    CHECK(t <= static_cast<long long>(after) + 2);
}

TEST_CASE("http.fetch via curl-impersonate") {
    LogSetup ls;
    Store store("test_plugin_fetch.db");
    plugin::Config c = test_conf();
    plugin::Host host(c);
    host.load_all(store);

    SUBCASE("unreachable host returns detectable failure") {
        const std::string res =
            host.call("fetcher", "fetch", "{\"url\":\"http://127.0.0.1:9/nope\",\"opts\":{\"timeout_ms\":500}}");
        CHECK(res.find("\"ok\":false") != std::string::npos);
    }

    if (const char* smoke = std::getenv("PLUGIN_SMOKE_URL"); smoke != nullptr) {
        const std::string url = std::string("{\"url\":\"") + smoke + "\",\"opts\":{}}";
        const std::string res = host.call("fetcher", "fetch", url);
        INFO(res);
        CHECK(res.find("\"ok\":true") != std::string::npos);
        CHECK(res.find("\"status\":200") != std::string::npos);
    }
}

TEST_CASE("health: last call outcome per plugin") {
    LogSetup ls;
    Store store("test_health.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    SUBCASE("ok call -> active, failing call -> error with message") {
        (void)host.call("good", "ping", "{}");
        CHECK(host.health("good").ok);
        (void)host.call("good", "fail", "{}"); // fixture returns ok=false boom
        const auto h = host.health("good");
        CHECK_FALSE(h.ok);
        CHECK(h.error == "boom");
        CHECK(h.at > 0);
    }

    SUBCASE("unknown plugin defaults to ok") {
        CHECK(host.health("ghost").ok);
    }
}

TEST_CASE("fs.read: confined read-only plugin file access") {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / "cppisekai_fsread_test";
    fs::remove_all(base);
    fs::create_directories(base / "sub");
    {
        std::ofstream o(base / "config.json");
        o << "{\"k\":1}";
    }
    {
        std::ofstream o(base / "sub" / "nested.txt");
        o << "nested";
    }

    CHECK(plugin::fs_read(base, "config.json").value_or("!") == "{\"k\":1}");
    CHECK(plugin::fs_read(base, "sub/nested.txt").value_or("!") == "nested");
    CHECK(plugin::fs_read(base, "./sub/../config.json").value_or("!") == "{\"k\":1}");
    CHECK(plugin::fs_read(base, "missing.txt") == std::nullopt);
    CHECK(plugin::fs_read(base, "../outside") == std::nullopt);
    CHECK(plugin::fs_read(base, "/etc/hostname") == std::nullopt);
    CHECK(plugin::fs_read(base, "") == std::nullopt);
    CHECK(plugin::fs_read(base, "sub") == std::nullopt);  // directory, not a regular file

    // Lua binding end-to-end: fixture plugin reads its own entry file.
    LogSetup ls;
    Store store("test_fsread.db");
    plugin::Host host(test_conf());
    host.load_all(store);
    const std::string res = host.call("fsread", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"len\"") != std::string::npos);
    const std::string esc = host.call("fsread", "escape", "{}");
    CHECK(esc.find("\"gone\":true") != std::string::npos);

    fs::remove_all(base);
    std::error_code ec;
    fs::remove("test_fsread.db", ec);
}

TEST_CASE("wasm runtime: ABI call over wasm3") {
    LogSetup ls;
    Store store("test_wasm.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("wasmfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"wasm\":\"pong\"") != std::string::npos);
    CHECK(host.health("wasmfix").ok);
}

TEST_CASE("wasm runtime: broken module loads as absent") {
    LogSetup ls;
    Store store("test_wasmbad.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    // wasmbad fails wasm parse => not registered; wasmfix in the same scan is.
    CHECK(host.call("wasmbad", "x", "{}").find("no plugin 'wasmbad'") != std::string::npos);
    CHECK(host.call("wasmfix", "ping", "{}").find("\"ok\":true") != std::string::npos);
}

TEST_CASE("wasm runtime: gas trap maps to ok:false") {
    LogSetup ls;
    Store store("test_wasmspin.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("wasmspin", "x", "{}");
    CHECK(res.find("\"ok\":false") != std::string::npos);
    CHECK_FALSE(host.health("wasmspin").ok);
}

TEST_CASE("js runtime: realm call over quickjs") {
    LogSetup ls;
    Store store("test_js.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("jsfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"js\":\"pong\"") != std::string::npos);
    CHECK(host.health("jsfix").ok);

    // budget: spin loops forever; the interrupt handler ends it ok:false
    const std::string spin = host.call("jsfix", "spin", "{}");
    CHECK(spin.find("\"ok\":false") != std::string::npos);
}

TEST_CASE("js runtime: broken entry loads as absent") {
    LogSetup ls;
    Store store("test_jsbad.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    // jsbad fails to parse => not registered; jsfix in the same scan is.
    CHECK(host.call("jsbad", "x", "{}").find("no plugin 'jsbad'") != std::string::npos);
    CHECK(host.call("jsfix", "ping", "{}").find("\"ok\":true") != std::string::npos);
}

TEST_CASE("fetch: family map, target list, profile guard") {
    CHECK(fetch::family_of("chrome116") == fetch::Family::Chrome);
    CHECK(fetch::family_of("ff117") == fetch::Family::Firefox);
    CHECK(fetch::family_of("firefox120") == fetch::Family::Firefox);
    CHECK(fetch::family_of("") == fetch::Family::Chrome);
    CHECK_FALSE(fetch::is_known_target("ff91esr")); // ff* builds unsupported (NSS vs BoringSSL), see fetch.hpp
    CHECK(fetch::is_known_target("safari15_3"));
    CHECK_FALSE(fetch::is_known_target("nope120"));
    CHECK(plugin::checked_profile("", "chrome116") == "chrome116");
    CHECK(plugin::checked_profile("bogus9", "chrome116") == "chrome116");
    CHECK(plugin::checked_profile("edge101", "chrome116") == "edge101"); // known target passes through
    CHECK(plugin::checked_profile("ff117", "chrome116") == "chrome116"); // ff* falls back to the chrome engine
}

TEST_CASE("fetch: cookie jar round-trip and per-plugin isolation") {
    std::error_code ec;
    const auto base = (std::filesystem::temp_directory_path() / "cppisekai_test_jars").string();
    std::filesystem::remove_all(base, ec);
    const std::string jar_a = plugin::cookie_jar_path("alpha", base);
    const std::string jar_b = plugin::cookie_jar_path("beta", base);
    CHECK(jar_a != jar_b);

    const auto page = std::filesystem::temp_directory_path() / "cppisekai_test_page.txt";
    {
        std::ofstream f(page);
        f << "hello";
    }

    fetch::Request req;
    req.url = "file://" + page.string(); // network-free: curl's local file handler
    req.cookie_jar = jar_a;
    const auto ra = fetch::sync(req, {});
    CHECK(ra.error.empty());
    CHECK(std::filesystem::exists(jar_a)); // jar written at handle cleanup

    const auto ra2 = fetch::sync(req, {}); // the reload path parses the existing jar
    CHECK(ra2.error.empty());

    req.cookie_jar = jar_b;
    const auto rb = fetch::sync(req, {});
    CHECK(rb.error.empty());
    CHECK(std::filesystem::exists(jar_b));
    CHECK(std::filesystem::exists(jar_a)); // alpha's jar untouched by beta's run
}

TEST_CASE("multi-file plugins: confined require() in lua and js") {
    LogSetup ls;
    Store store("test_modtest.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string lua_res = host.call("lua_modtest", "ping", "{}");
    CHECK(lua_res.find("\"doubled\":42") != std::string::npos);
    CHECK(lua_res.find("\"msg\":\"modul-ok\"") != std::string::npos);

    const std::string js_res = host.call("js_modtest", "ping", "{}");
    CHECK(js_res.find("\"doubled\":42") != std::string::npos);
    CHECK(js_res.find("\"msg\":\"modul-ok\"") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove("test_modtest.db", ec);
}

TEST_CASE("js host.url_*: shared codec, strings only") {
    LogSetup ls;
    Store store("test_jsurl.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("jsfix", "url", "{\"s\":\"x/y z\"}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"enc\":\"a%20b%26c%3Dd%25%3F\"") != std::string::npos);
    CHECK(res.find("\"dec\":\"a b&c=d%?\"") != std::string::npos);
    CHECK(res.find("\"rt\":\"x/y z\"") != std::string::npos);

    const std::string bad = host.call("jsfix", "urlbad", "{}");
    CHECK(bad.find("\"ok\":true") != std::string::npos);
    CHECK(bad.find("url_encode(string)") != std::string::npos);
}

TEST_CASE("wasm host.url_*: imports round-trip via shared codec") {
    LogSetup ls;
    Store store("test_wasmurl.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("wasmurl", "url", "{}");
    CHECK_MESSAGE(res.find("\"ok\":true") != std::string::npos, res);
    CHECK(res.find("\"enc\":\"a%20b%26c%3Dd%25%3F\"") != std::string::npos);
    CHECK(res.find("\"rt\":\"a b&c=d%?\"") != std::string::npos);
}

TEST_CASE("js host.config.get: plugin-local config, nil parity") {
    LogSetup ls;
    Store store("test_jsconf.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    const std::string res = host.call("js_modtest", "cfg", "{}");
    CHECK_MESSAGE(res.find("\"ok\":true") != std::string::npos, res);
    CHECK(res.find("\"v\":\"modul-conf\"") != std::string::npos);
    CHECK(res.find("\"n\":7") != std::string::npos);
    CHECK(res.find("\"miss\":null") != std::string::npos); // absent key
}

TEST_CASE("js host.crypto.vrf_sign: golden vector, null on failure") {
    LogSetup ls;
    Store store("test_jsvrf.db");
    plugin::Host host(test_conf());
    host.load_all(store);

    // same golden as the lua crypto fixture (tests/test_vrf.cpp "empty" vector)
    const std::string res = host.call("jsfix", "vrf", "{\"path\":\"/titles\",\"params\":{}}");
    CHECK_MESSAGE(res.find("\"ok\":true") != std::string::npos, res);
    CHECK(res.find("\"sig\":\"8sK3xtqdFQ\"") != std::string::npos);

    const std::string bad = host.call("jsfix", "vrfbad", "{}");
    CHECK_MESSAGE(bad.find("\"ok\":true") != std::string::npos, bad);
    CHECK(bad.find("\"badkey\":null") != std::string::npos); // core rejects bad base64
    CHECK(bad.find("\"nested\":null") != std::string::npos); // non-string param value
    CHECK(bad.find("\"noconf\":null") != std::string::npos); // absent config.json
}
