#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <glaze/glaze.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "http/stale.hpp"
#include "store/store.hpp"

namespace fs = std::filesystem;

namespace {

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_stale_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

Store store_at(const fs::path& root) { return Store((root / "test.db").string()); }

// The seam: scripted call outcomes; empty script slot = failing call.
struct FakeCall {
    std::string good;  // ok:true envelope, handed out while `succeed` holds
    std::string error; // ok:false envelope
    bool succeed{true};
    int calls{};

    std::string operator()() {
        ++calls;
        return succeed ? good : error;
    }
};

// Substring shields against key-order churn while still proving data + flag.
bool has(std::string_view hay, std::string_view needle) { return hay.find(needle) != std::string_view::npos; }

} // namespace

TEST_CASE("stale: success persists, then failing call serves last-good + cached:true") {
    TempRoot root;
    Store store = store_at(root.path);
    FakeCall call{R"({"ok":true,"data":{"titles":["one piece"]}})", R"({"ok":false,"error":"offline"})"};

    const std::string first = call_cached(store, "mangadex", "search", R"({"q":"one piece"})", std::ref(call));
    CHECK(has(first, R"("ok":true)"));
    CHECK(call.calls == 1);
    // age the row past ttl so the failure path (not the fresh look-aside) drives the second call
    store.api_cache_put("mangadex", stale_key("mangadex", "search", R"({"q":"one piece"})"),
                        R"({"q":"one piece"})", first, 1, 0);

    call.succeed = false; // plugin goes dark; same query, second attempt
    const std::string second = call_cached(store, "mangadex", "search", R"({"q":"one piece"})", std::ref(call));
    REQUIRE(has(second, R"("cached":true)")); // flag injected, top level
    CHECK(has(second, R"("titles":["one piece"])")); // last-good body intact
    CHECK(has(second, R"("ok":true)"));
    CHECK(call.calls == 2); // failure path still called once, then served stale
}

TEST_CASE("stale: fresh row within ttl is served without calling the plugin") {
    TempRoot root;
    Store store = store_at(root.path);
    FakeCall call{R"({"ok":true,"data":{"n":1}})", R"({"ok":false,"error":"offline"})"};

    CHECK(call_cached(store, "m", "search", R"({"q":"x"})", std::ref(call)) != "{}");
    CHECK(call.calls == 1);
    const std::string second = call_cached(store, "m", "search", R"({"q":"x"})", std::ref(call));
    CHECK(call.calls == 1); // fresh hit: no second plugin call
    CHECK(has(second, R"("n":1)"));
    CHECK(!has(second, "cached")); // fresh serve carries no flag
}

TEST_CASE("stale: expired row triggers refresh, row replaced") {
    TempRoot root;
    Store store = store_at(root.path);
    // ttl_s <= 0 => always expired (test hook via direct put)
    store.api_cache_put("m", stale_key("m", "search", R"({"q":"x"})"), R"({"q":"x"})",
                        R"({"ok":true,"data":{"n":"old"}})", 1, 0);
    FakeCall call{R"({"ok":true,"data":{"n":"new"}})", R"({"ok":false,"error":"offline"})"};

    const std::string res = call_cached(store, "m", "search", R"({"q":"x"})", std::ref(call));
    CHECK(call.calls == 1); // plugin ran despite a row existing
    CHECK(has(res, R"("n":"new")"));
    const auto row = store.api_cache_get("m", stale_key("m", "search", R"({"q":"x"})"));
    REQUIRE(row.has_value());
    CHECK(has(row->payload, R"("n":"new")"));
    CHECK(row->ttl_s == kApiCacheTtl); // re-written with real ttl
}

TEST_CASE("stale: permuted query order is one entry") {
    TempRoot root;
    Store store = store_at(root.path);
    FakeCall call{R"({"ok":true,"data":{"n":1}})", R"({"ok":false,"error":"offline"})"};

    const std::string first = call_cached(store, "m", "chapters", R"({"a":"1","b":"2"})", std::ref(call));
    // age past ttl so stale path is exercised
    store.api_cache_put("m", stale_key("m", "chapters", R"({"a":"1","b":"2"})"),
                        R"({"a":"1","b":"2"})", first, 1, 0);
    call.succeed = false;
    const std::string stale = call_cached(store, "m", "chapters", R"({"b":"2","a":"1"})", std::ref(call));
    CHECK(has(stale, R"("cached":true)")); // same entry despite key order
    CHECK(has(stale, R"("n":1)"));
}

TEST_CASE("stale: method outside opt-in passes through, nothing cached") {
    TempRoot root;
    Store store = store_at(root.path);
    FakeCall call{R"({"ok":true,"data":{}})", "", true};

    const std::string res = call_cached(store, "m", "feed", R"({"x":"1"})", std::ref(call));
    CHECK(res == call.good); // verbatim, no envelope shaping
    CHECK_FALSE(store.api_cache_get("m", stale_key("m", "feed", R"({"x":"1"})")).has_value());
    CHECK(call.calls == 1);
}

TEST_CASE("stale: failure with no last-good returns the error envelope") {
    TempRoot root;
    Store store = store_at(root.path);
    FakeCall call{"", R"({"ok":false,"error":"offline"})", false};

    const std::string res = call_cached(store, "m", "detail", R"({"id":"7"})", std::ref(call));
    CHECK(has(res, R"("ok":false)")); // honest failure, no fake cached flag
    CHECK(!has(res, "cached"));
}
