#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <BS_thread_pool.hpp>
#include <crow.h>
#include <glaze/glaze.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "http/enrich_internal.hpp"
#include "http/http_routes.hpp"
#include "http/stale.hpp"
#include "store/store.hpp"

namespace fs = std::filesystem;
using http::api::apply_chosen;
using http::api::normalize_chosen;
using http::api::stored_source;
using http::api::valid_title_pair;

namespace {

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_enrich_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

Store store_at(const fs::path& root) { return Store((root / "test.db").string()); }

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

void seed_saved(Store& store, const std::string& plugin, const std::string& slug) {
    store.save_manga({plugin, slug, "Name " + slug, "https://cover/" + slug + ".jpg", 0});
}

} // namespace

TEST_CASE("enrich: stored_source finds the right source entry and reports fetched_at") {
    const std::string fetched = R"([
        {"source":"mangadex","kind":"detail","fetched_at":1000,"data":{"title":"MD"}},
        {"source":"mangadex","kind":"covers","fetched_at":1500,"data":{"covers":["c1"]}},
        {"source":"mangaupdates","fetched_at":2000,"data":{"title":"MU"}}
    ])";
    std::int64_t at = 0;
    const auto md = stored_source(fetched, "mangadex", "detail", at);
    REQUIRE(md.has_value());
    CHECK(at == 1000);
    const std::string md_json = md->dump();
    CHECK(has(md_json, "MD"));
    const auto cov = stored_source(fetched, "mangadex", "covers", at);
    REQUIRE(cov.has_value());
    CHECK(at == 1500);
    const auto none = stored_source(fetched, "nope", "detail", at);
    CHECK_FALSE(none.has_value());
    // legacy detail entry (no kind) is served for "detail" but not "covers"
    const auto legacy = stored_source(fetched, "mangaupdates", "detail", at);
    CHECK(legacy.has_value());
    CHECK_FALSE(stored_source(fetched, "mangaupdates", "covers", at).has_value());
    const auto garbage = stored_source("not json{{", "mangadex", "detail", at);
    CHECK_FALSE(garbage.has_value());
}

TEST_CASE("enrich: normalize_chosen whitelist and type rules") {
    const auto good = crow::json::load(R"({"title":"T","artist":"Solo","tags_add":["a","b"],"tags_hidden":["x"]})");
    REQUIRE(good);
    const auto out = normalize_chosen(good);
    REQUIRE(out.has_value());
    CHECK(has(*out, R"("title":"T")"));
    CHECK(has(*out, R"("artist":["Solo"])")); // String normalized to array

    const auto unknown = crow::json::load(R"({"bogus":"x"})");
    CHECK_FALSE(normalize_chosen(unknown).has_value());

    const auto bad_tag = crow::json::load(R"({"tags_add":[1,2]})");
    CHECK_FALSE(normalize_chosen(bad_tag).has_value());

    const auto bad_artist = crow::json::load(R"({"artist":[1]})");
    CHECK_FALSE(normalize_chosen(bad_artist).has_value());

    const auto list_artist = crow::json::load(R"({"artist":["a","b"]})");
    const auto norm = normalize_chosen(list_artist);
    REQUIRE(norm.has_value());
    CHECK(has(*norm, R"("artist":["a","b"])"));
}

TEST_CASE("enrich: apply_chosen merges and leaves envelope intact when no row") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    const std::string plugin = "enrichtest";
    const std::string slug = "some-slug";

    const std::string envelope = R"({"ok":true,"cached":false,"data":{"name":"Source","description":"d","tags":["a","B","c"],"authors":["src-author"],"artists":[]}})";

    // no row -> envelope untouched
    CHECK(apply_chosen(store, db, plugin, slug, envelope) == envelope);

    seed_saved(store, plugin, slug);
    store.title_meta_put_fetched(plugin, slug,
        R"([{"source":"mangadex","fetched_at":1111,"data":{"title":"MDT"}}])", 1111);
    store.title_meta_put_chosen(plugin, slug,
        R"({"title":"Chosen","tags_add":["New"],"tags_hidden":["a"],"cover":"https://c/x.jpg"})");

    const auto merged = apply_chosen(store, db, plugin, slug, envelope);
    CHECK(has(merged, R"("name":"Chosen")"));
    CHECK(has(merged, R"("chosen_cover":"https://c/x.jpg")"));
    // hidden tag removed (case-insensitive), add appended, dedupe by lower
    CHECK_FALSE(has(merged, R"("a","B")")); // 'a' hidden
    CHECK(has(merged, R"("New")"));
    CHECK(has(merged, R"("fetched_at":1111)"));
    CHECK(has(merged, R"("enrichment")"));
}

TEST_CASE("enrich: enrichment writes never touch saved_manga / api_cache") {
    TempRoot root;
    Store store = store_at(root.path);
    const std::string plugin = "enrichtest";
    const std::string slug = "untouched-slug";

    seed_saved(store, plugin, slug);
    const auto saved_before = store.saved_get(plugin, slug);
    REQUIRE(saved_before.has_value());
    const std::string saved_name_before = saved_before->name;
    const std::string saved_cover_before = saved_before->cover;

    store.api_cache_put(plugin, "hash-key", R"({"q":"x"})", R"({"ok":true,"data":{}})", 1234, 9999);
    const auto cache_before = store.api_cache_get(plugin, "hash-key");
    REQUIRE(cache_before.has_value());
    const std::string payload_before = cache_before->payload;

    // enrichment writes
    store.title_meta_put_fetched(plugin, slug, R"([{"source":"mangadex","fetched_at":42,"data":{}}])", 42);
    store.title_meta_put_chosen(plugin, slug, R"({"title":"X","cover":"https://c/y.jpg"})");
    store.title_meta_reset_cover(plugin, slug);
    store.title_meta_reset_detail(plugin, slug);

    const auto saved_after = store.saved_get(plugin, slug);
    REQUIRE(saved_after.has_value());
    CHECK(saved_after->name == saved_name_before);
    CHECK(saved_after->cover == saved_cover_before);

    const auto cache_after = store.api_cache_get(plugin, "hash-key");
    REQUIRE(cache_after.has_value());
    CHECK(cache_after->payload == payload_before);
    CHECK(cache_after->fetched_at == 1234);
}

TEST_CASE("enrich: valid_title_pair accepts plugin-minted case, rejects traversal") {
    // atsumaru regression: its ids carry uppercase (H6P2Z) and were 404-rejected.
    CHECK(valid_title_pair("atsumaru", "H6P2Z"));
    CHECK(valid_title_pair("mangadex", "a4304d3b-191d-471f-8c78-353d403a35d0"));
    CHECK(valid_title_pair("mangadex", "ch.1-5"));
    CHECK(!valid_title_pair("mangadex", ""));
    CHECK(!valid_title_pair("mangadex", "a/b"));
    CHECK(!valid_title_pair("mangadex", ".."));
    CHECK(!valid_title_pair("mangadex", "a..b"));
    CHECK(!valid_title_pair("mangadex", "a b"));
    CHECK(!valid_title_pair("mangadex", std::string(200, 'a')));
}
