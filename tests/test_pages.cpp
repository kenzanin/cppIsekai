#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "http/pages_api.hpp"

namespace {

// Substring shield against key-order churn; find-positions prove ordering.
bool has(std::string_view hay, std::string_view needle) { return hay.find(needle) != std::string_view::npos; }

const std::string chs_env = R"({"ok":true,"data":[)"
                            R"({"id":"c1","number":1},)"
                            R"({"id":"c2","number":2},)"
                            R"({"id":"c3","number":3}]})";

} // namespace

TEST_CASE("pages: ordered host-rendered urls with prev/next neighbors") {
    const std::string pages_env = R"({"ok":true,"data":["u?1","b","c"]})";
    const auto out = build_pages_response(chs_env, pages_env, {"m", "s", 2});
    REQUIRE(out);
    const std::string& body = *out;
    const auto p1 = body.find("page=1");
    const auto p2 = body.find("page=2");
    const auto p3 = body.find("page=3");
    REQUIRE(p1 != std::string::npos);
    REQUIRE(p2 != std::string::npos);
    REQUIRE(p3 != std::string::npos);
    CHECK(p1 < p2); // list order == page order, 1-based
    CHECK(p2 < p3);
    CHECK(has(body, "/cache/img?url=u%3F1")); // query chars of the page url encoded
    CHECK(has(body, "&plugin=m&slug=s&ch=2&page=1")); // full host context on every entry
    CHECK(has(body, R"("prev_ch":1)"));
    CHECK(has(body, R"("next_ch":3)"));
    CHECK(has(body, R"("chapter":2)"));
}

TEST_CASE("pages: boundary chapters omit the missing neighbor") {
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    const auto first = build_pages_response(chs_env, pages_env, {"m", "s", 1});
    REQUIRE(first);
    CHECK(has(*first, R"("next_ch":2)"));
    CHECK(!has(*first, "prev_ch"));

    const auto last = build_pages_response(chs_env, pages_env, {"m", "s", 3});
    REQUIRE(last);
    CHECK(has(*last, R"("prev_ch":2)"));
    CHECK(!has(*last, "next_ch"));
}

TEST_CASE("pages: unknown chapter, empty pages or malformed envelope -> nullopt") {
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    CHECK(!build_pages_response(chs_env, pages_env, {"m", "s", 9}).has_value()); // no chapter 9
    CHECK(!build_pages_response(chs_env, R"({"ok":true,"data":[]})", {"m", "s", 1}).has_value());
    CHECK(!build_pages_response("not json", pages_env, {"m", "s", 1}).has_value());
    CHECK(!build_pages_response(chs_env, "not json", {"m", "s", 1}).has_value());
    CHECK(!build_pages_response(R"({"ok":false,"error":"x"})", pages_env, {"m", "s", 1}).has_value());
}
