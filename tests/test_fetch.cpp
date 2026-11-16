// fetch-gate unit tests: pure helpers only -- no live sockets (D6 regression).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "browser/mcp.hpp"
#include "fetch/fetch.hpp"

#include <doctest/doctest.h>
#include <spdlog/spdlog.h>

TEST_CASE("loopback url detection for the proxy exemption") {
    CHECK(fetch::is_loopback_url("http://127.0.0.1:8899/mcp"));
    CHECK(fetch::is_loopback_url("http://localhost:8090/"));
    CHECK(fetch::is_loopback_url("http://[::1]:8890/mcp")); // v6 bracket + port
    CHECK(fetch::is_loopback_url("http://localhost"));      // no port, no path

    CHECK_FALSE(fetch::is_loopback_url("https://comix.to/api/v1/chapters/1"));
    CHECK_FALSE(fetch::is_loopback_url("http://127.0.0.2:8899/mcp")); // near-loopback is not loopback
    CHECK_FALSE(fetch::is_loopback_url("localhost:8899"));            // no scheme: gate always has one
    CHECK_FALSE(fetch::is_loopback_url("http://localhost.example/x")); // suffix host
}

TEST_CASE("mcp client against the live daemon when present (1.1/1.2)") {
    browser::configure("http://127.0.0.1:8899", "");
    std::string err;
    const std::string g = browser::call("goto", R"({"url":"data:text/html,<title>ok</title>"})", &err);
    if (g.empty() && err.find("Couldn't connect") != std::string::npos) {
        MESSAGE("lightpanda mcp not running on 8899; live leg skipped");
        return;
    }
    const std::string ok = browser::call("evaluate", R"({"script":"1+1"})", &err);

    SUBCASE("evaluate echoes a value") { CHECK(ok == "2"); }
    SUBCASE("bogus tool surfaces the rpc error string (1.2)") {
        std::string e2;
        CHECK(browser::call("no_such_tool", "{}", &e2).empty());
        CHECK(e2 == "Tool not found");
    }
    SUBCASE("non-json args fail fast") {
        std::string e3;
        CHECK(browser::call("evaluate", "not-json", &e3).empty());
        CHECK(e3 == "browser: args is not a json object");
    }
}
