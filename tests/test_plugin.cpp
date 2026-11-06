#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <ctime>
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
    if (const char* smoke = std::getenv("PLUGIN_SMOKE_URL"); smoke != nullptr)
        ; // live smoke exercised separately below when env set
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
