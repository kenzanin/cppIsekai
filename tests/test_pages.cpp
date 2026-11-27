#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <string_view>

#include "http/pages_api.hpp"
#include "http/pages_source.hpp"

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

TEST_CASE("pages: offline envelopes rebuilt from stored rows keep a cached chapter readable") {
    // With no network, the route rebuilds these two envelopes from chapters /
    // chapter_pages rows; the same shapes must compose into a normal response.
    const auto stored_chs = chapters_envelope_from_rows({{1, "a1"}, {2, "a2"}, {3, "a3"}});
    const auto stored_pages = pages_envelope_from_urls({"https://cdn/a/1.jpg", "https://cdn/a/2.jpg"});
    const auto out = build_pages_response(stored_chs, stored_pages, {"m", "s", 2});
    REQUIRE(out);
    CHECK(has(*out, R"("ch_id":"a2")"));
    CHECK(has(*out, R"("prev_ch":1)"));
    CHECK(has(*out, R"("next_ch":3)"));
    CHECK(has(*out, "url=https%3A%2F%2Fcdn%2Fa%2F1.jpg"));
    CHECK(has(*out, "&page=2"));
    // No stored rows and a failed fetch still means unknown chapter.
    CHECK_FALSE(build_pages_response(stored_chs, R"({"ok":false,"error":"offline"})", {"m", "s", 2}));
}

TEST_CASE("pages: stored rows and a parse render the same response") {
    // pages-local-first D1: the route answers from chapter_pages rows instead of
    // a plugin parse, so both envelopes must compose into one identical body.
    const auto stored_chs = chapters_envelope_from_rows({{24, "c24a"}, {24, "c24b"}});
    const std::vector<std::string> urls{"https://cdn/x/1.jpg", "https://cdn/x/2.jpg?a=b"};
    const std::string parsed = R"({"ok":true,"data":["https://cdn/x/1.jpg","https://cdn/x/2.jpg?a=b"]})";
    const auto from_rows = build_pages_response(stored_chs, pages_envelope_from_urls(urls), {"m", "s", 24, "c24b"});
    const auto from_parse = build_pages_response(stored_chs, parsed, {"m", "s", 24, "c24b"});
    REQUIRE(from_rows);
    REQUIRE(from_parse);
    CHECK(*from_rows == *from_parse);
}

TEST_CASE("pages: descending plugin order still yields nearest neighbors") {
    // mangafreak emits newest-first; walking the array used to make 36/1 win for every chapter.
    const std::string desc = R"({"ok":true,"data":[)"
                             R"({"id":"c36","number":36},)"
                             R"({"id":"c24b","number":24},)"
                             R"({"id":"c24a","number":24},)"
                             R"({"id":"c23","number":23},)"
                             R"({"id":"c3","number":3},)"
                             R"({"id":"c2","number":2},)"
                             R"({"id":"c1","number":1}]})";
    const std::string pages_env = R"({"ok":true,"data":["a"]})";

    const auto second = build_pages_response(desc, pages_env, {"m", "s", 2});
    REQUIRE(second);
    CHECK(has(*second, R"("prev_ch":1)"));
    CHECK(has(*second, R"("next_ch":3)"));

    const auto dup = build_pages_response(desc, pages_env, {"m", "s", 23});
    REQUIRE(dup);
    CHECK(has(*dup, R"("prev_ch":3)"));
    CHECK(has(*dup, R"("next_ch":24)")); // duplicate numbers stay consecutive stops
    CHECK(has(*dup, R"("next_ch_id":"c24a")")); // ties read in id order: a before b
}

TEST_CASE("pages: hidden chapters are stepped over in both directions") {
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    // c2 is skip-flagged (hidden in the chapter list): the reader must never
    // land on it, so 1 -> 3 directly and 3 -> 1 directly.
    const auto from1 = build_pages_response(chs_env, pages_env, {"m", "s", 1, "", {"c2"}});
    REQUIRE(from1);
    CHECK(has(*from1, R"("next_ch":3)"));
    CHECK(has(*from1, R"("next_ch_id":"c3")"));

    const auto from3 = build_pages_response(chs_env, pages_env, {"m", "s", 3, "", {"c2"}});
    REQUIRE(from3);
    CHECK(has(*from3, R"("prev_ch":1)"));
    CHECK(has(*from3, R"("prev_ch_id":"c1")"));

    // A hidden target still serves its own pages; only the walk steps over it.
    const auto hidden_self = build_pages_response(chs_env, pages_env, {"m", "s", 2, "c2", {"c2"}});
    REQUIRE(hidden_self);
    CHECK(has(*hidden_self, R"("prev_ch":1)"));
    CHECK(has(*hidden_self, R"("next_ch":3)"));
}

TEST_CASE("pages: hidden neighbors leave no next at the end of the list") {
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    const auto out = build_pages_response(chs_env, pages_env, {"m", "s", 1, "", {"c2", "c3"}});
    REQUIRE(out);
    CHECK(!has(*out, "next_ch")); // everything after chapter 1 is hidden
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

TEST_CASE("pages: duplicate numbers are individually reachable by id") {
    const std::string dup = R"({"ok":true,"data":[)"
                             R"({"id":"c23","number":23},)"
                             R"({"id":"c24a","number":24},)"
                             R"({"id":"c24b","number":24},)"
                             R"({"id":"c25","number":25}]})"; // two releases numbered 24
    const std::string pages_env = R"({"ok":true,"data":["a"]})";

    const auto first = build_pages_response(dup, pages_env, {"m", "s", 24, "c24a"});
    REQUIRE(first);
    CHECK(has(*first, R"("ch_id":"c24a")"));
    CHECK(has(*first, R"("prev_ch":23)"));
    CHECK(has(*first, R"("prev_ch_id":"c23")"));
    CHECK(has(*first, R"("next_ch":24)")); // sibling, same number
    CHECK(has(*first, R"("next_ch_id":"c24b")"));

    const auto second = build_pages_response(dup, pages_env, {"m", "s", 24, "c24b"});
    REQUIRE(second);
    CHECK(has(*second, R"("ch_id":"c24b")"));
    CHECK(has(*second, R"("prev_ch":24)")); // sibling, same number
    CHECK(has(*second, R"("prev_ch_id":"c24a")"));
    CHECK(has(*second, R"("next_ch":25)"));
    CHECK(has(*second, R"("next_ch_id":"c25")"));
}

TEST_CASE("pages: number-only path still resolves to the first entry (legacy)") {
    const std::string dup = R"({"ok":true,"data":[)"
                             R"({"id":"c24a","number":24},)"
                             R"({"id":"c24b","number":24}]})";
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    const auto out = build_pages_response(dup, pages_env, {"m", "s", 24, ""});
    REQUIRE(out);
    CHECK(has(*out, R"("ch_id":"c24a")")); // first number match keeps the old behaviour

    const auto fallback = build_pages_response(dup, pages_env, {"m", "s", 24, "no-such-id"});
    REQUIRE(fallback);
    CHECK(has(*fallback, R"("ch_id":"c24a")")); // unknown ch_id falls back the same way
}

TEST_CASE("pages: unknown chapter, empty pages or malformed envelope -> nullopt") {
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    CHECK(!build_pages_response(chs_env, pages_env, {"m", "s", 9}).has_value()); // no chapter 9
    CHECK(!build_pages_response(chs_env, R"({"ok":true,"data":[]})", {"m", "s", 1}).has_value());
    CHECK(!build_pages_response("not json", pages_env, {"m", "s", 1}).has_value());
    CHECK(!build_pages_response(chs_env, "not json", {"m", "s", 1}).has_value());
    CHECK(!build_pages_response(R"({"ok":false,"error":"x"})", pages_env, {"m", "s", 1}).has_value());
}

TEST_CASE("pages: chapter 0 is a valid chapter (falsy-zero regression)") {
    const std::string chs_with_zero = R"({"ok":true,"data":[)"
                                      R"({"id":"c0","number":0},)"
                                      R"({"id":"c1","number":1},)"
                                      R"({"id":"c2","number":2}]})";
    const std::string pages_env = R"({"ok":true,"data":["a0","a1"]})";
    const auto out = build_pages_response(chs_with_zero, pages_env, {"m", "s", 0});
    REQUIRE(out);
    CHECK(has(*out, R"("chapter":0)"));
    CHECK(has(*out, R"("next_ch":1)"));
    CHECK(!has(*out, "prev_ch"));
    CHECK(has(*out, "ch=0&page=1")); // fmt_ch: integral 0, not 0.000000
    CHECK(!has(*out, "ch=0.000000"));
}

TEST_CASE("pages: fractional chapter kept in out chapter and url") {
    const std::string chs_frac = R"({"ok":true,"data":[)"
                                 R"({"id":"c1","number":1},)"
                                 R"({"id":"c15","number":1.5},)"
                                 R"({"id":"c2","number":2}]})";
    const std::string pages_env = R"({"ok":true,"data":["a"]})";
    const auto out = build_pages_response(chs_frac, pages_env, {"m", "s", 1.5});
    REQUIRE(out);
    CHECK(has(*out, R"("chapter":1.5)"));
    CHECK(has(*out, "ch=1.5&page=1"));
    CHECK(has(*out, R"("prev_ch":1)"));
    CHECK(has(*out, R"("next_ch":2)"));
}

TEST_CASE("pages-local-first: opened chapter answers from rows, plugin untouched") {
    std::vector<std::string> parsed;
    const auto src = http::api::pick_pages_source(
        chs_env, 2, "",
        [](const std::string& id) {
            return id == "c2" ? std::vector<std::string>{"https://cdn/2/p1.jpg", "https://cdn/2/p2.jpg"}
                              : std::vector<std::string>{};
        },
        [&](const std::string& id) {
            parsed.push_back(id);
            return std::string{R"({"ok":true,"data":["live"]})"};
        });
    CHECK(src.id == "c2");
    CHECK(src.from_rows);
    CHECK(parsed.empty()); // D1: rows are the durable list, no plugin call
    const auto page_urls = http::api::pages_urls(src.envelope);
    REQUIRE(page_urls.size() == 2);
    CHECK(page_urls[1] == "https://cdn/2/p2.jpg"); // parse order kept
    const auto out = build_pages_response(chs_env, src.envelope, {"m", "s", 2, src.id});
    REQUIRE(out);
    CHECK(has(*out, R"("ch_id":"c2")"));
    CHECK(has(*out, R"("next_ch":3)"));
}

TEST_CASE("pages-local-first: cold chapter parses once and reports no rows") {
    std::vector<std::string> parsed;
    const auto src = http::api::pick_pages_source(
        chs_env, 2, "",
        [](const std::string&) { return std::vector<std::string>{}; },
        [&](const std::string& id) {
            parsed.push_back(id);
            return std::string{R"({"ok":true,"data":["live1"]})"};
        });
    CHECK(src.id == "c2");
    CHECK_FALSE(src.from_rows); // the route writes these urls into chapter_pages
    REQUIRE(parsed.size() == 1); // no rows -> one parse, siblings of 2 are none
    CHECK(parsed[0] == "c2");
    CHECK(http::api::pages_urls(src.envelope) == std::vector<std::string>{"live1"});
}

TEST_CASE("pages-local-first: a flaky release falls back to its same-number sibling") {
    const std::string dup = R"({"ok":true,"data":[)"
                            R"({"id":"c24a","number":24},)"
                            R"({"id":"c24b","number":24},)"
                            R"({"id":"c25","number":25}]})";
    std::vector<std::string> parsed;
    const auto src = http::api::pick_pages_source(
        dup, 24, "",
        [](const std::string&) { return std::vector<std::string>{}; },
        [&](const std::string& id) {
            parsed.push_back(id);
            // comico's CF origin 502s only some releases' open call
            return id == "c24a" ? std::string{R"({"ok":false,"error":"502"})"}
                                : std::string{R"({"ok":true,"data":["b1"]})"};
        });
    CHECK(src.id == "c24b");
    CHECK_FALSE(src.from_rows);
    CHECK(parsed == std::vector<std::string>{"c24a", "c24b"}); // id-ascending walk
    CHECK(http::api::envelope_error(R"({"ok":false,"error":"502"})") == "502"); // real reason for the 404
}

TEST_CASE("pages-local-first: ch_id picks the duplicate whose rows exist") {
    const std::string dup = R"({"ok":true,"data":[)"
                            R"({"id":"c24a","number":24},)"
                            R"({"id":"c24b","number":24}]})";
    std::vector<std::string> parsed;
    const auto src = http::api::pick_pages_source(
        dup, 24, "c24b", [](const std::string& id) {
            return id == "c24b" ? std::vector<std::string>{"b-rows1"} : std::vector<std::string>{};
        },
        [&](const std::string& id) {
            parsed.push_back(id);
            return std::string{R"({"ok":true,"data":["a-live"]})"};
        });
    CHECK(src.id == "c24b");
    CHECK(src.from_rows);
    CHECK(parsed.empty()); // the named release had rows: nothing else is asked
}

TEST_CASE("pages-local-first: unknown chapter yields no source") {
    int calls = 0;
    const auto src = http::api::pick_pages_source(
        chs_env, 9, "", [](const std::string&) { return std::vector<std::string>{}; },
        [&](const std::string&) {
            ++calls;
            return std::string{R"({"ok":true,"data":["x"]})"};
        });
    CHECK(src.id.empty());
    CHECK_FALSE(src.from_rows);
    CHECK(calls == 0);
    CHECK(http::api::pick_pages_source("not json", 1, "", [](const std::string&) { return std::vector<std::string>{}; },
                                       [&](const std::string&) {
                                           ++calls;
                                           return std::string{R"({"ok":true,"data":["x"]})"};
                                       })
              .id.empty());
    CHECK(calls == 0);
}

TEST_CASE("pages-local-first: sibling walk is id-order and capped at three") {
    const std::string many = R"({"ok":true,"data":[)"
                             R"({"id":"e","number":7},{"id":"d","number":7},)"
                             R"({"id":"c","number":7},{"id":"b","number":7},)"
                             R"({"id":"a","number":7},{"id":"z","number":8}]})";
    const auto sibs = http::api::sibling_ids(many, 7, "c");
    CHECK(sibs == std::vector<std::string>{"a", "b", "d"}); // primary skipped, c dropped by the cap
    CHECK(http::api::sibling_ids(many, 8, "z").empty());
}
