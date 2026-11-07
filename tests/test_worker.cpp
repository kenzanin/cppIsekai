#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cache/cache.hpp"
#include "reader/worker.hpp"

namespace fs = std::filesystem;

namespace {

// Canned bodies per key; anything absent fails the fetch (hermetic offline).
struct FakeState {
    std::map<std::string, std::string> bodies;
    std::vector<std::string> fetched; // call order, read after stop() joins

    std::optional<std::string> get(const std::string& key) {
        fetched.push_back(key);
        const auto it = bodies.find(key);
        return it == bodies.end() ? std::nullopt : std::optional{it->second};
    }
};

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_worker_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

struct Rig {
    TempRoot dir;
    FakeState fake;
    cache::Cache cache{cache::Config{dir.path.string(), 1 << 20},
                       [this](const std::string& k) { return fake.get(k); }};

    void seed(const std::string& key) { REQUIRE(cache.store(key, "img", "body:" + key).has_value()); }
    bool on_disk(const std::string& key) { return cache.peek(key, "img").has_value(); }
};

} // namespace

TEST_CASE("worker: delta only -- seeded urls are skipped, misses fetched in window order") {
    Rig rig;
    rig.seed("w/a");
    rig.seed("w/c");
    rig.fake.bodies = {{"w/b", "b"}, {"w/d", "d"}, {"w/e", "e"}};

    // Worker construction starts the thread; the queue is the only shared
    // state, so nudge -> stop is a deterministic single-batch run: stop
    // drains before join returns.
    reader::Worker worker{rig.cache};
    worker.nudge({"w/a", "w/b", "w/c", "w/d", "w/e"});
    worker.stop();

    CHECK(rig.fake.fetched == std::vector<std::string>{"w/b", "w/d", "w/e"});
    for (const auto& url : {"w/a", "w/b", "w/c", "w/d", "w/e"}) CHECK(rig.on_disk(url));
}

TEST_CASE("worker: failed fetch is best-effort, the rest of the window still lands") {
    Rig rig;
    rig.fake.bodies = {{"w/a", "a"}}; // w/missing not in bodies -> fetch fails

    reader::Worker worker{rig.cache};
    worker.nudge({"w/missing", "w/a"});
    worker.stop();

    CHECK(rig.fake.fetched == std::vector<std::string>{"w/missing", "w/a"});
    CHECK(rig.on_disk("w/a"));
    CHECK(!rig.on_disk("w/missing"));
}

TEST_CASE("worker: start/stop sanity -- idle stop and reuse after stop are quiet") {
    Rig rig;
    {
        reader::Worker worker{rig.cache}; // never nudged: plain start/join
    }
    reader::Worker worker{rig.cache};
    worker.stop(); // idempotent: the destructor stops again
    CHECK(rig.fake.fetched.empty());
}
