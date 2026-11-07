#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <glaze/glaze.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "http/stale.hpp"

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

cache::Cache cache_with(const std::string& root) {
    return cache::Cache(cache::Config{root, 1 << 20}, {});
}

// Substring shields against key-order churn while still proving data + flag.
bool has(std::string_view hay, std::string_view needle) { return hay.find(needle) != std::string_view::npos; }

} // namespace

TEST_CASE("stale: success persists, then failing call serves last-good + cached:true") {
    TempRoot root;
    cache::Cache cache = cache_with(root.path.string());
    FakeCall call{R"({"ok":true,"data":{"titles":["one piece"]}})", R"({"ok":false,"error":"offline"})"};

    const std::string first = call_cached(cache, "mangadex", "search", R"({"q":"one piece"})", std::ref(call));
    CHECK(has(first, R"("ok":true)"));
    CHECK(call.calls == 1);

    call.succeed = false; // plugin goes dark; same query, second attempt
    const std::string second = call_cached(cache, "mangadex", "search", R"({"q":"one piece"})", std::ref(call));
    REQUIRE(has(second, R"("cached":true)")); // flag injected, top level
    CHECK(has(second, R"("titles":["one piece"])")); // last-good body intact
    CHECK(has(second, R"("ok":true)"));
    CHECK(call.calls == 2); // failure path still called once, then served stale
}

TEST_CASE("stale: permuted query order is one entry") {
    TempRoot root;
    cache::Cache cache = cache_with(root.path.string());
    FakeCall call{R"({"ok":true,"data":{"n":1}})", R"({"ok":false,"error":"offline"})"};

    CHECK(call_cached(cache, "m", "chapters", R"({"a":"1","b":"2"})", std::ref(call)) != "{}");
    call.succeed = false;
    const std::string stale = call_cached(cache, "m", "chapters", R"({"b":"2","a":"1"})", std::ref(call));
    CHECK(has(stale, R"("cached":true)")); // same entry despite key order
    CHECK(has(stale, R"("n":1)"));
}

TEST_CASE("stale: method outside opt-in passes through, nothing cached") {
    TempRoot root;
    cache::Cache cache = cache_with(root.path.string());
    FakeCall call{R"({"ok":true,"data":{}})", "", true};

    const std::string res = call_cached(cache, "m", "feed", R"({"x":"1"})", std::ref(call));
    CHECK(res == call.good); // verbatim, no envelope shaping
    CHECK(fs::is_empty(root.path / "api")); // no cache writes on pass-through
}

TEST_CASE("stale: failure with no last-good returns the error envelope") {
    TempRoot root;
    cache::Cache cache = cache_with(root.path.string());
    FakeCall call{"", R"({"ok":false,"error":"offline"})", false};

    const std::string res = call_cached(cache, "m", "detail", R"({"id":"7"})", std::ref(call));
    CHECK(has(res, R"("ok":false)")); // honest failure, no fake cached flag
    CHECK(!has(res, "cached"));
}
