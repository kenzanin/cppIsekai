#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "cache/cache.hpp"
#include "cache/sha1.hpp"

namespace fs = std::filesystem;

namespace {

// Canned bodies per key; anything absent fails the fetch (hermetic offline).
struct FakeState {
    std::map<std::string, std::string> bodies;
    int calls{};

    std::optional<std::string> get(const std::string& key) {
        ++calls;
        const auto it = bodies.find(key);
        if (it == bodies.end()) return std::nullopt;
        return it->second;
    }
};

std::string forty(char c) { return std::string(40, c); } // 3 x 40B > 100B cap

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_cache_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

bool ok(const std::optional<std::string>& p) { return p && fs::exists(*p); }

// Seed the tree by hand (boot-walk / purge fixtures bypass the cache).
void put(const fs::path& p, const std::string& body) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    REQUIRE(!ec);
    std::ofstream out(p, std::ios::binary);
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    REQUIRE(out.good());
}

// Backdate mtime so boot seeds recency deterministically.
void age(const fs::path& p, std::chrono::hours h) {
    std::error_code ec;
    fs::last_write_time(p, fs::last_write_time(p, ec) - h, ec);
    REQUIRE(!ec);
}

} // namespace

TEST_CASE("cache: sha1 known vectors") {
    CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_hex("The quick brown fox jumps over the lazy dog") ==
          "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST_CASE("cache: miss fetch+store, hit serves without refetch") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"http://x/a.jpg", forty('a')}, {"http://x/p.PNG?w=100", forty('p')}};
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    const auto a = cache.ensure("http://x/a.jpg", "img");
    REQUIRE(ok(a));
    CHECK(fs::path(*a).extension() == ".jpg"); // ext from url, lowercased
    CHECK(fs::path(*a).parent_path().filename() == "img");
    CHECK(fs::path(*a).stem() == sha1_hex("http://x/a.jpg")); // sha1 keys
    CHECK(fs::file_size(*a) == 40);

    const int calls = state->calls;
    CHECK(cache.ensure("http://x/a.jpg", "img") == a); // same path on hit
    CHECK(state->calls == calls);                      // no refetch on hit

    CHECK(fs::path(*cache.ensure("http://x/p.PNG?w=100", "img")).extension() == ".png");
}

TEST_CASE("cache: evict on insert follows LRU, survivors stay readable") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"a", forty('a')}, {"b", forty('b')}, {"c", forty('c')}};
    cache::Cache cache(cache::Config{root.path.string(), 100}, // tiny cap
                       [state](const std::string& k) { return state->get(k); });

    const auto a = cache.ensure("a", "img");
    const auto b = cache.ensure("b", "img");
    REQUIRE(ok(a));
    REQUIRE(ok(b));

    cache.ensure("a", "img"); // touch a: b becomes LRU
    const auto c = cache.ensure("c", "img");
    REQUIRE(ok(c));

    CHECK(state->calls == 3);    // touch + insert, no extra fetches
    CHECK_FALSE(fs::exists(*b)); // LRU evicted from disk...
    CHECK(fs::exists(*a));       // ...recently-used survivor...
    const int calls = state->calls;
    CHECK(cache.ensure("a", "img") == a);
    CHECK(state->calls == calls); // ...and still a hit, not a refetch

    CHECK(ok(cache.ensure("b", "img"))); // evicted key refetches fine
}

TEST_CASE("cache: restart rescans disk, counts toward cap, keeps recency") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"a", forty('a')}, {"c", forty('c')}, {"d", forty('d')}};
    const cache::Config conf{root.path.string(), 100};
    cache::Cache first(conf, [state](const std::string& k) { return state->get(k); });

    const auto a = first.ensure("a", "img");
    const auto c = first.ensure("c", "img");
    REQUIRE(ok(a));
    REQUIRE(ok(c));
    // distinct mtimes so the rebuilt recency order is deterministic
    std::error_code ec;
    fs::last_write_time(*a, fs::last_write_time(*a) - std::chrono::hours(2), ec);
    REQUIRE(!ec);

    cache::Cache again(conf, [state](const std::string& k) { return state->get(k); });
    const int calls = state->calls;
    CHECK(again.ensure("a", "img") == a); // rebooted entries serve from disk
    CHECK(state->calls == calls);

    const auto d = again.ensure("d", "img"); // 120B > 100B: evict, LRU = c
    REQUIRE(ok(d));                          // (a's hit above bumped its recency)
    CHECK(state->calls == calls + 1);
    CHECK_FALSE(fs::exists(*c)); // rebuilt pool counted a+c, evicts by LRU
    CHECK(fs::exists(*a));
    CHECK(fs::exists(*d));
}

TEST_CASE("cache: api pool separates; failed fetch leaves nothing behind") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"mangadex|chapters|q1", forty('j')}};
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    const auto j = cache.ensure("mangadex|chapters|q1", "api");
    REQUIRE(ok(j));
    CHECK(fs::path(*j).parent_path().filename() == "api");

    CHECK(cache.ensure("http://x/none.jpg", "img") == std::nullopt);
    std::size_t files = 0;
    std::error_code nodir; // img pool created lazily on first store
    for (const auto& e : fs::directory_iterator(root.path / "img", nodir)) {
        static_cast<void>(e);
        ++files; // no stray .tmp / partial file from the failed insert
    }
    CHECK(files == 0);
}

TEST_CASE("cache: rel_path nests the file, peek/ensure honor it, api ignores it") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"http://x/a.jpg", forty('a')}};
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    const std::string rel = "mangadex/slug-x/ch003/p002.jpg";
    const auto p = cache.store("http://x/a.jpg", "img", forty('a'), rel);
    REQUIRE(ok(p));
    CHECK(*p == (root.path / "img" / rel).string()); // exact structured landing
    CHECK(cache.peek("http://x/a.jpg", "img", rel).value_or("") == forty('a'));

    const int calls = state->calls; // store never fetches
    CHECK(cache.ensure("http://x/a.jpg", "img", rel) == p); // same entry on hit
    CHECK(state->calls == calls);

    // Index is on-disk-path keyed: a second rel for the same sha refetches and
    // lands beside the first; neither copy is dropped.
    const auto q = cache.ensure("http://x/a.jpg", "img", "mangadex/slug-x/ch003/p001.jpg");
    REQUIRE(ok(q));
    CHECK(q != p);
    CHECK(state->calls == calls + 1);
    CHECK(fs::exists(*p));

    // api stays sha-keyed whatever rel it is handed (stale layer, D7).
    const auto j = cache.store("mangadex|chapters|q", "api", "{}", "ignored/rel.json");
    REQUIRE(ok(j));
    CHECK(fs::path(*j).parent_path().filename() == "api");
    CHECK(fs::path(*j).stem() == sha1_hex("mangadex|chapters|q"));
}

TEST_CASE("cache: boot walk spans tree depth; trim evicts across pools by mtime") {
    TempRoot root;
    const auto deep = root.path / "img" / "mangadex" / "slug-x" / "ch001" / "p001.jpg";
    const auto flat = root.path / "img" / (sha1_hex("f") + ".png");
    const auto apif = root.path / "api" / (sha1_hex("q") + ".json");
    put(deep, forty('d'));
    put(flat, forty('f'));
    put(apif, forty('j'));
    age(apif, std::chrono::hours(3)); // oldest: flat api pool
    age(deep, std::chrono::hours(2)); // then the nested img page
    // flat img stays newest; boot seeds recency in mtime order over the whole tree

    auto state = std::make_shared<FakeState>();
    state->bodies = {{"n", forty('n')}};
    cache::Cache cache(cache::Config{root.path.string(), 100},
                       [state](const std::string& k) { return state->get(k); });

    const auto n = cache.ensure("n", "img"); // 160B > 100B: two oldest evicted
    REQUIRE(ok(n));
    CHECK(state->calls == 1);
    CHECK_FALSE(fs::exists(apif)); // cross-pool LRU victim
    CHECK_FALSE(fs::exists(deep)); // nested depth was indexed, then evicted
    CHECK(fs::exists(flat));
    CHECK(fs::exists(*n));
}

TEST_CASE("cache: one-time purge flattens v1 pools; rollback copy swept next boot") {
    TempRoot root;
    const auto img = root.path / "img";
    put(img / (sha1_hex("u") + ".png"), forty('u')); // pure sha-flat: the v1 signature
    put(root.path / ".legacy-api-old" / "x.json", "{}"); // stale rollback, even earlier boot

    cache::Cache first(cache::Config{root.path.string(), 1 << 20}, {});
    CHECK_FALSE(fs::exists(img));                                // pool renamed away...
    CHECK(fs::exists(root.path / ".legacy-img"));                // ...under the rollback name
    CHECK(fs::exists(root.path / ".legacy-img" / (sha1_hex("u") + ".png")));
    CHECK_FALSE(fs::exists(root.path / ".legacy-api-old"));      // earlier copy swept now

    put(img / (sha1_hex("v") + ".png"), forty('v')); // flat pool refills post-purge
    cache::Cache again(cache::Config{root.path.string(), 1 << 20}, {});
    CHECK(fs::exists(img / (sha1_hex("v") + ".png")));           // purge is once: stamped
    CHECK_FALSE(fs::exists(root.path / ".legacy-img"));          // prior copy swept

    // Non-sha names are live data, not residue: untouched, stamp or not.
    TempRoot live;
    put(live.path / "api" / "custom.json", "{}");
    cache::Cache third(cache::Config{live.path.string(), 1 << 20}, {});
    CHECK(fs::exists(live.path / "api" / "custom.json"));
}


// V3 (cache-layout-v3): context-aware landing paths, sidecar, extension-free lookup.
TEST_CASE("cache v3: page landing paths + duplicate chapter disambiguation") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    cache::Cache::PageCtx pg{.plugin = "mangadex", .slug = "solo-leveling",
                             .chapter = 12, .page = 3, .page_id = "8f3a2c1b9d"};
    const auto p = cache.img_store("http://x/p3.png", "B", {}, pg);
    REQUIRE(ok(p));
    CHECK(*p == (root.path / "img" / "mangadex" / "manga" / "solo-leveling" /
                 "ch012-88230349" / "p003.png").string());

    // same ch12, different chapter id -> sibling dir, both survive
    cache::Cache::PageCtx dup = pg;
    dup.page_id = "00deadbeef";
    const auto q = cache.img_store("http://x/p3b.png", "C", {}, dup);
    REQUIRE(ok(q));
    CHECK(fs::path(*q).parent_path().filename() == "ch012-b5850973");
    CHECK(fs::exists(*p));
    CHECK(fs::exists(*q));

    // zero-pad: ch 2 -> ch002; empty page_id falls back to the key for id8
    cache::Cache::PageCtx p2 = pg;
    p2.chapter = 2; p2.page = 1; p2.page_id.clear();
    const auto r = cache.img_store("http://x/p1.jpg", "D", {}, p2);
    REQUIRE(ok(r));
    CHECK(fs::path(*r).parent_path().filename().string().rfind("ch002-9266cd27", 0) == 0);

    // no context -> sha fallback at pool root
    const auto f = cache.img_store("http://x/plain.gif", "E", {}, {});
    REQUIRE(ok(f));
    CHECK(fs::path(*f).parent_path().filename() == "img");
}

TEST_CASE("cache v3: cover landing + sidecar round-trip") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    cache::Cache::CoverCtx cv{.url = "http://x/cover.png", .w = 800, .h = 1200, .cover = true};
    const auto c = cache.img_store("http://x/cover.png", "F", cv, {});
    REQUIRE(ok(c));
    CHECK(fs::path(*c).parent_path().filename() == "cover");
    // ratio rides in the filename as fixed-point 4 digits: 800/1200 -> 6667
    CHECK(fs::path(*c).filename().string().find("-6667.") != std::string::npos);
    CHECK(!fs::exists(*c + ".json")); // no sidecar files anymore
    CHECK(cache::Cache::ratio_of_name(fs::path(*c).filename().string()) > 0.66);
    CHECK(cache::Cache::ratio_of_name(fs::path(*c).filename().string()) < 0.67);

    // cover without dims -> "-0" suffix, ratio parse yields 0.0 (no header)
    cache::Cache::CoverCtx nodims{.url = "http://x/c2.png", .cover = true};
    const auto c2 = cache.img_store("http://x/c2.png", "G", nodims, {});
    REQUIRE(ok(c2));
    CHECK(cache::Cache::ratio_of_name(fs::path(*c2).filename().string()) == 0.0);
}

TEST_CASE("cache v3: cover_serve is extension-free; absent -> nullopt") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"http://x/cv.jpg", forty('i')}};
    cache::Cache cache(cache::Config{root.path.string(), 1 << 20},
                       [state](const std::string& k) { return state->get(k); });

    cache::Cache::CoverCtx cv{.url = "http://x/cv.jpg", .plugin = "mangadex",
                              .w = 100, .h = 200, .cover = true};
    REQUIRE(ok(cache.img_store("http://x/cv.jpg", "H", cv, {})));
    REQUIRE(cache.cover_serve("mangadex", "http://x/cv.jpg").has_value());
    CHECK(!cache.cover_serve("mangadex", "http://x/other.jpg").has_value());
}
