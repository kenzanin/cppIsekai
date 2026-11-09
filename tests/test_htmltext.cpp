// Host-tool parity + helper core tests (change plugin-helper, task 2.1).
// Network-free. Core scenarios run directly on plugin::htmltext; the
// runtime-parity cases run the migrated lua/js/wasm fixtures through the
// real Host (same pattern as tests/test_plugin.cpp).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "plugin/htmltext.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {
plugin::Config test_conf() {
    plugin::Config c;
    c.dir = TEST_PLUGIN_FIXTURES;
    c.instruction_budget = 200000;
    return c;
}
} // namespace

TEST_CASE("htmltext: re2 match/find groups (spec scenarios)") {
    const auto m = plugin::re2_match(R"(ch(\d+))", "ch1050");
    REQUIRE(m.size() == 2);
    CHECK(m[0] == "ch1050");
    CHECK(m[1] == "1050");

    const auto f = plugin::re2_find(R"((\w+)@(\w+))", "user@host other");
    REQUIRE(f.size() == 3);
    CHECK(f[0] == "user@host");
    CHECK(f[1] == "user");
    CHECK(f[2] == "host");
}

TEST_CASE("htmltext: re2 no-match is empty; match anchors; bad pattern throws") {
    CHECK(plugin::re2_find(R"(zzz(\d+))", "no digits here").empty());
    CHECK(plugin::re2_match(R"(ch(\d+))", "Chapter ch1050").empty());

    bool threw = false;
    try {
        (void)plugin::re2_find("(unclosed", "x");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE("htmltext: html select/attr (spec scenarios)") {
    const std::string html = R"(<html><body>
      <div class="title"><a href="/manga/a">Alpha   </a></div>
      <div class="title"><a href="/manga/b"> Beta</a></div>
      <div class="title"><a>None</a></div>
    </body></html>)";

    const auto texts = plugin::html_select(html, "div.title > a");
    REQUIRE(texts.size() == 3);
    CHECK(texts[0] == "Alpha"); // trimmed
    CHECK(texts[1] == "Beta");
    CHECK(texts[2] == "None");

    const auto hrefs = plugin::html_attr(html, "div.title > a", "href");
    REQUIRE(hrefs.size() == 2); // attr-less anchor skipped
    CHECK(hrefs[0] == "/manga/a");
    CHECK(hrefs[1] == "/manga/b");
    CHECK(plugin::html_attr(html, "div.title > a", "missing").empty());
}

TEST_CASE("htmltext: lenient on bad HTML / selector") {
    CHECK(plugin::html_select("<div><b>broken", "div").size() == 1);
    CHECK(plugin::html_select("<b>x</b>", "div##bad[[ sel").empty());
    CHECK(plugin::html_select("", "div").empty());
}

TEST_CASE("host: lua parity fixture (host.* resolves, no bare globals)") {
    plugin::Host host{test_conf()};
    Store store("test_htmltext.db");
    CHECK(host.load_all(store) >= 1);
    // good fixture migrated to host.* (host.log.info / host.now).
    const std::string res = host.call("good", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
}

TEST_CASE("host: js parity fixture (host.* resolves)") {
    plugin::Host host{test_conf()};
    Store store("test_htmltext.db");
    host.load_all(store);
    const std::string res = host.call("jsfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"js\":\"pong\"") != std::string::npos);
}

TEST_CASE("host: wasm parity fixture (host.* via env.* imports)") {
    plugin::Host host{test_conf()};
    Store store("test_htmltext.db");
    host.load_all(store);
    const std::string res = host.call("wasmfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"wasm\":\"pong\"") != std::string::npos);
}
