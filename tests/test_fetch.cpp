// fetch-gate unit tests: pure helpers only -- no live sockets (D6 regression).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "browser/chrome.hpp"
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

TEST_CASE("chrome backend args validation (offline legs)") {
    browser::configure(""); // auto-discover; no launch happens for arg errors
    std::string err;
    SUBCASE("non-json args fail fast") {
        CHECK(browser::call("evaluate", "not-json", &err).empty());
        CHECK(err == "browser: args is not a json object");
    }
    SUBCASE("unknown tool named in the error") {
        // Unbooted backend: boot attempt would spawn chrome -- acceptable here,
        // it exits the machine unchanged; assert only the error contract.
        CHECK(browser::call("no_such_tool", R"({"x":1})", &err).empty());
        const bool named = err.find("unknown tool") != std::string::npos;
        const bool nobrowser = err.find("launch failed") != std::string::npos;
        CHECK((named || nobrowser));
    }
}
