#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cache/cache.hpp"
#include "cache/sha1.hpp"
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
    bool on_disk_rel(const std::string& key, const std::string& rel) {
        return cache.peek(key, "img", rel).has_value();
    }
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

TEST_CASE("worker: structured placement -- complete ctx lands <plugin>/<slug>/chNNN/pNNN.<ext>") {
    Rig rig;
    rig.fake.bodies = {{"http://x/1.jpg", "1"}, {"http://x/2.jpg", "2"}, {"http://y/1.jpg", "3"}};

    // One full window: two pages of ch001. Page numbers are the 1-based
    // position in the batch, so ch001/p001 < ch001/p002 in lex order.
    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://x/1.jpg", "http://x/2.jpg"}, reader::WindowCtx{"md", "slug-a", 1});
        worker.stop();
    }
    const auto r1 = "md/slug-a/ch001-edd003c0/p001.jpg";
    const auto r2 = "md/slug-a/ch001-c3142807/p002.jpg";
    CHECK(rig.on_disk_rel("http://x/1.jpg", r1));
    CHECK(rig.on_disk_rel("http://x/2.jpg", r2));

    // Second window of the same manga: one page of ch002. Two separate
    // windows (each drained on stop) keep ch001 and ch002 side by side.
    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://y/1.jpg"}, reader::WindowCtx{"md", "slug-a", 2});
        worker.stop();
    }
    const auto r3 = "md/slug-a/ch002-e106a54b/p001.jpg";
    CHECK(rig.on_disk_rel("http://y/1.jpg", r3));
    // Zero-pad keeps ch001..ch010 in reading order across a full chapter span.
    const std::vector<std::string> dirs = {"ch001", "ch002", "ch010"};
    for (std::size_t i = 1; i < dirs.size(); ++i) CHECK(dirs[i - 1] < dirs[i]);
}

TEST_CASE("worker: sha fallback -- missing slug or chapter stays on the exact old path") {
    Rig rig;
    rig.fake.bodies = {{"http://x/a.png", "a"}};
    const reader::WindowCtx ctx{"md", "slug-a", 1};

    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/a.png"}); // no ctx at all
        w.stop();
    }
    CHECK(rig.on_disk("http://x/a.png")); // flat <sha>.png, no structured copy
    CHECK(!rig.on_disk_rel("http://x/a.png", "md/slug-a/ch001/p001.png"));

    // ctx present but slug missing -> same flat path.
    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/a.png"}, reader::WindowCtx{"md", "", 1});
        w.stop();
    }
    CHECK(rig.on_disk("http://x/a.png"));
    CHECK(!rig.on_disk_rel("http://x/a.png", "md//ch001/p001.png"));

    // ctx present but url has no extension -> structured layout needs the
    // url's ext, so it stays on the flat path.
    rig.fake.bodies["http://x/plain"] = "p";
    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/plain"}, ctx);
        w.stop();
    }
    CHECK(rig.on_disk("http://x/plain"));
    CHECK(!rig.on_disk_rel("http://x/plain", "md/slug-a/ch001/p001.bin"));
}

TEST_CASE("worker: store reuse -- peek with the same rel_path hits, a second rel refetches") {
    Rig rig;
    rig.fake.bodies = {{"http://x/a.jpg", "body-a"}};
    const reader::WindowCtx ctx{"md", "slug-a", 1};

    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://x/a.jpg"}, ctx); // ch001 window
        worker.stop(); // coalescing is newest-wins: drain before the next nudge
    }
    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://x/a.jpg"}, reader::WindowCtx{"md", "slug-a", 2}); // second rel
        worker.stop();
    }
    const auto ra1 = "md/slug-a/ch001-6d46c90d/p001.jpg";
    const auto ra2 = "md/slug-a/ch002-6d46c90d/p001.jpg";
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra1));
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra2));
    const int calls = rig.fake.fetched.size();
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra1)); // peek = hit, no fetch
    CHECK(rig.fake.fetched.size() == calls);
}
