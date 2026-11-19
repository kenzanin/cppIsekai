// Host-tool parity + helper core tests (change plugin-helper, task 2.1).
// Network-free. Core scenarios run directly on plugin::htmltext; the
// runtime-parity cases run the migrated lua/js/wasm fixtures through the
// real Host (same pattern as tests/test_plugin.cpp).

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "plugin/htmltext.hpp"
#include "plugin/host_text.hpp"
#include "plugin/url_codec.hpp"
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
    // destruction order: host (declared after store) goes first, so the
    // store outlives the Lua runtimes that reference it.
    Store store("test_htmltext.db");
    plugin::Host host{test_conf()};
    CHECK(host.load_all(store) >= 1);
    // good fixture migrated to host.* (host.log.info / host.now).
    const std::string res = host.call("good", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
}

TEST_CASE("host: js parity fixture (host.* resolves)") {
    Store store("test_htmltext.db");
    plugin::Host host{test_conf()};
    host.load_all(store);
    const std::string res = host.call("jsfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"js\":\"pong\"") != std::string::npos);
}

TEST_CASE("host: wasm parity fixture (host.* via env.* imports)") {
    Store store("test_htmltext.db");
    plugin::Host host{test_conf()};
    host.load_all(store);
    const std::string res = host.call("wasmfix", "ping", "{}");
    CHECK(res.find("\"ok\":true") != std::string::npos);
    CHECK(res.find("\"wasm\":\"pong\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// plugin-host-promotion 6.1: new host core helpers
// ---------------------------------------------------------------------------

TEST_CASE("host_text: normalize_status keys collapse + synonyms + passthrough") {
    using plugin::host_text::normalize_status;
    CHECK(normalize_status("On-Going") == "Ongoing");
    CHECK(normalize_status("on_going") == "Ongoing");
    CHECK(normalize_status("  Releasing ") == "Ongoing");
    CHECK(normalize_status("publish") == "Ongoing");
    CHECK(normalize_status("Published") == "Ongoing");
    CHECK(normalize_status("completed") == "Completed");
    CHECK(normalize_status("finished") == "Completed");
    CHECK(normalize_status("On-Hiatus") == "Hiatus");
    CHECK(normalize_status("onhold") == "Hiatus");
    CHECK(normalize_status("Cancelled") == "Dropped");
    CHECK(normalize_status("Upcoming") == "Upcoming");
    CHECK(normalize_status("NotYetReleased") == "Upcoming");
    CHECK(normalize_status("") == "unknown");
    CHECK(normalize_status("###") == "unknown");
    CHECK(normalize_status("Season Break") == "Season Break"); // unrecognized passes through
}

TEST_CASE("host_text: date_epoch layouts + garbage") {
    using plugin::host_text::date_epoch;
    // Fixed instances: 2024-01-02T03:04:05Z == 1704164645
    CHECK(date_epoch("2024-01-02T03:04:05Z").value_or(-1) == 1704164645);
    CHECK(date_epoch("2024-01-02 03:04:05").value_or(-1) == 1704164645);
    CHECK(date_epoch("2024-01-02").value_or(-1) == 1704153600); // midnight UTC
    CHECK(date_epoch("Jan 2, 2024").value_or(-1) == 1704153600);
    CHECK_FALSE(date_epoch("not a date").has_value());
    CHECK_FALSE(date_epoch("").has_value());
}

TEST_CASE("host_text: json_blob balanced scan") {
    using plugin::host_text::json_blob;
    const std::string doc = R"(prefix var X = {"a":1,"b":{"c":[2,3]},"s":"}"} suffix)";
    CHECK(json_blob(doc, "var X =").value_or("") == R"({"a":1,"b":{"c":[2,3]},"s":"}"})");
    CHECK(json_blob("no marker here {\"k\":1}", "").value_or("") == "{\"k\":1}");
    CHECK(json_blob("arr [1,[2,{\"x\":\"]\"}]] t", "").value_or("") == R"([1,[2,{"x":"]"}]])");
    CHECK_FALSE(json_blob("unclosed {\"a\":1", "").has_value());
    CHECK_FALSE(json_blob("no braces", "zzz").has_value());
    CHECK_FALSE(json_blob("{\"a':1}", "").has_value()); // string-brace imbalance guard
}

TEST_CASE("host_text: url_abs four branches (madaradex canonical)") {
    using plugin::url_abs;
    const std::string base = "https://madaradex.org/manga/foo/ch-1";
    CHECK(url_abs(base, "https://cdn.ex/a.png") == "https://cdn.ex/a.png");
    CHECK(url_abs(base, "//cdn.ex/a.png") == "https://cdn.ex/a.png");
    CHECK(url_abs(base, "/covers/a.webp") == "https://madaradex.org/covers/a.webp");
    CHECK(url_abs(base, "x/y.jpg") == "https://madaradex.org/manga/foo/x/y.jpg");
    CHECK(url_abs(base, "") == "");
    CHECK(url_abs("no-scheme", "/p") == "/p");
}

TEST_CASE("htmltext: re2_capture group / no-group / no-match") {
    auto cap = plugin::re2_capture(R"rx(src="([^"]+)")rx", "<img src=\"a.png\">");
    CHECK(cap.value_or("") == "a.png");
    cap = plugin::re2_capture("ch(\\d+)x(\\d+)", "ch12x3");
    CHECK(cap.value_or("") == "12");
    CHECK(plugin::re2_capture("(zzz)x", "abc").has_value() == false);
    CHECK(plugin::re2_capture("ch\\d+", "ch9").value_or("") == "ch9");
}
