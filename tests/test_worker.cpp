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
                       [this](const std::string& k, const std::string&) { return fake.get(k); }};

    // The img pool has no sha fallback, so seeded/peeked files need a
    // placement path: seed into the exact slot a worker batch would fill.
    static std::string slot(int page) {
        std::string n = std::to_string(page);
        while (n.size() < 3) n = "0" + n;
        return "md/manga/slug-a/ch001/p" + n + ".bin";
    }
    void seed(const std::string& key, int page) {
        REQUIRE(cache.store(key, "img", "body:" + key, slot(page)).has_value());
    }
    bool on_disk(const std::string& key, int page) { return cache.peek(key, "img", slot(page)).has_value(); }
    bool on_disk_rel(const std::string& key, const std::string& rel) {
        return cache.peek(key, "img", rel).has_value();
    }
};

} // namespace

TEST_CASE("worker: delta only -- seeded urls are skipped, misses fetched in window order") {
    Rig rig;
    rig.seed("w/a", 1);
    rig.seed("w/c", 3);
    rig.fake.bodies = {{"w/b", "b"}, {"w/d", "d"}, {"w/e", "e"}};

    // Worker construction starts the thread; the queue is the only shared
    // state, so nudge -> stop is a deterministic single-batch run: stop
    // drains before join returns.
    reader::Worker worker{rig.cache};
    worker.nudge({"w/a", "w/b", "w/c", "w/d", "w/e"}, reader::WindowCtx{"md", "slug-a", 1});
    worker.stop();

    CHECK(rig.fake.fetched == std::vector<std::string>{"w/b", "w/d", "w/e"});
    int page = 0;
    for (const auto& url : {"w/a", "w/b", "w/c", "w/d", "w/e"}) CHECK(rig.on_disk(url, ++page));
}

TEST_CASE("worker: failed fetch is best-effort, the rest of the window still lands") {
    Rig rig;
    rig.fake.bodies = {{"w/a", "a"}}; // w/missing not in bodies -> fetch fails

    reader::Worker worker{rig.cache};
    worker.nudge({"w/missing", "w/a"}, reader::WindowCtx{"md", "slug-a", 1});
    worker.stop();

    CHECK(rig.fake.fetched == std::vector<std::string>{"w/missing", "w/a"});
    CHECK(rig.on_disk("w/a", 2));
    CHECK(!rig.on_disk("w/missing", 1));
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

TEST_CASE("worker: structured placement -- complete ctx lands <plugin>/manga/<slug>/chNNN/pNNN.<ext>") {
    Rig rig;
    rig.fake.bodies = {{"http://x/1.jpg", "1"}, {"http://x/2.jpg", "2"}, {"http://y/1.jpg", "3"}};

    // One full window: two pages of ch001. Page numbers are the 1-based
    // position in the batch, so ch001/p001 < ch001/p002 in lex order.
    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://x/1.jpg", "http://x/2.jpg"}, reader::WindowCtx{"md", "slug-a", 1});
        worker.stop();
    }
    const auto r1 = "md/manga/slug-a/ch001/p001.jpg";
    const auto r2 = "md/manga/slug-a/ch001/p002.jpg";
    CHECK(rig.on_disk_rel("http://x/1.jpg", r1));
    CHECK(rig.on_disk_rel("http://x/2.jpg", r2));

    // Second window of the same manga: one page of ch002. Two separate
    // windows (each drained on stop) keep ch001 and ch002 side by side.
    {
        reader::Worker worker{rig.cache};
        worker.nudge({"http://y/1.jpg"}, reader::WindowCtx{"md", "slug-a", 2});
        worker.stop();
    }
    const auto r3 = "md/manga/slug-a/ch002/p001.jpg";
    CHECK(rig.on_disk_rel("http://y/1.jpg", r3));
    // Zero-pad keeps ch001..ch010 in reading order across a full chapter span.
    const std::vector<std::string> dirs = {"ch001", "ch002", "ch010"};
    for (std::size_t i = 1; i < dirs.size(); ++i) CHECK(dirs[i - 1] < dirs[i]);
}

TEST_CASE("worker: a nudge without full placement context is dropped, never sha-written") {
    Rig rig;
    rig.fake.bodies = {{"http://x/a.png", "a"}, {"http://x/plain", "p"}};

    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/a.png"}); // no ctx at all
        w.stop();
    }
    CHECK(rig.fake.fetched.empty()); // not even fetched
    CHECK(!rig.on_disk("http://x/a.png", 1));

    // ctx present but slug missing -> same: dropped, nothing written.
    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/a.png"}, reader::WindowCtx{"md", "", 1});
        w.stop();
    }
    CHECK(rig.fake.fetched.empty());
    CHECK(!rig.on_disk("http://x/a.png", 1));

    // Full ctx, url without an extension -> ext_of falls back to "bin" and the
    // structured landing still applies.
    {
        reader::Worker w{rig.cache};
        w.nudge({"http://x/plain"}, reader::WindowCtx{"md", "slug-a", 1});
        w.stop();
    }
    CHECK(rig.on_disk_rel("http://x/plain", "md/manga/slug-a/ch001/p001.bin"));
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
    // Passthrough transcode (fake body is not a real image) keeps the key's
    // ext, so the structured landing path is taken. Ch002 ctx -> sibling dir.
    const auto ra1 = "md/manga/slug-a/ch001/p001.jpg";
    const auto ra2 = "md/manga/slug-a/ch002/p001.jpg";
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra1));
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra2));
    const int calls = rig.fake.fetched.size();
    CHECK(rig.on_disk_rel("http://x/a.jpg", ra1)); // peek = hit, no fetch
    CHECK(rig.fake.fetched.size() == calls);
}
