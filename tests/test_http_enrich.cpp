#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <BS_thread_pool.hpp>
#include <crow.h>
#include <glaze/glaze.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "enrich/enrich_internal.hpp"
#include "http/http_routes.hpp"
#include "http/stale.hpp"
#include "store/store.hpp"

namespace fs = std::filesystem;
using http::api::apply_chosen;
using http::api::materialize_detail;
using http::api::normalize_chosen;
using http::api::saved_detail_response;
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

TEST_CASE("enrich: apply_chosen derives detail name from the scraper title") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    // scraper envelopes carry `title` only; the detail h1 reads `name`
    const std::string envelope = R"({"ok":true,"data":{"title":"Scraper Title","description":"d"}})";
    const auto out = apply_chosen(store, db, "enrichtest", "not-in-library", envelope);
    CHECK(has(out, R"("name":"Scraper Title")"));
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
    // 4ucomics ids ARE the site slug; its 202-char one used to 404 the whole
    // title (detail/chapters/pages). Ceiling is NAME_MAX, not a display length.
    CHECK(valid_title_pair(
        "4ucomics",
        "the-strongest-demon-king-at-0-months-old-im-a-reincarnator-with-a-cheat-ability-that-makes-me-grow-stronger"
        "-just-by-eating-but-since-im-a-baby-ill-grow-up-on-the-heroes-breast-milk-and-become-invincible"));
    CHECK(valid_title_pair("mangadex", std::string(200, 'a')));
    CHECK(!valid_title_pair("mangadex", std::string(256, 'a')));
    // unicode slugs pass now (host handles utf8); apostrophe/exclamation/colon too
    CHECK(valid_title_pair("sanascans", "a-contract-marriage-as-the-marquis's-villainous-wife"));
    CHECK(valid_title_pair("sanascans", "i'll-divorce-you-gracefully!"));
    CHECK(valid_title_pair("sanascans", "escaping-my-charming-sister:-a-mind-reading-lady's-path"));
    CHECK(valid_title_pair("mangadex", "海贼王"));
    CHECK(valid_title_pair("mangadex", "pO3vrpjr|one-piece"));
    CHECK(!valid_title_pair("mangadex", "a%2fb"));
    CHECK(!valid_title_pair("mangadex", "a\\b"));
    CHECK(!valid_title_pair("mangadex", "a?b"));
    CHECK(!valid_title_pair("mangadex", "a#b"));
    CHECK(!valid_title_pair("mangadex", "a\x7f" "b"));
    CHECK(!valid_title_pair("mangadex", "a\x1f" "b"));
}

TEST_CASE("enrich: normalize_chosen related_add validation") {
    const auto good = crow::json::load(R"({"related_add":[{"name":"Solo Leveling","url":"https://x/y","id":"a1"}]})");
    REQUIRE(good);
    const auto out = normalize_chosen(good);
    REQUIRE(out.has_value());
    CHECK(has(*out, R"("name":"Solo Leveling")"));
    CHECK(has(*out, R"("url":"https://x/y")"));

    const auto missing_url = crow::json::load(R"({"related_add":[{"name":"x"}]})");
    CHECK_FALSE(normalize_chosen(missing_url).has_value());

    const auto empty_name = crow::json::load(R"({"related_add":[{"name":"","url":"u"}]})");
    CHECK_FALSE(normalize_chosen(empty_name).has_value());

    const auto unknown_key = crow::json::load(R"({"related_add":[{"name":"x","url":"u","bogus":1}]})");
    CHECK_FALSE(normalize_chosen(unknown_key).has_value());

    const auto not_array = crow::json::load(R"({"related_add":"u"})");
    CHECK_FALSE(normalize_chosen(not_array).has_value());
}

TEST_CASE("enrich: materialize_detail write-through and read-first serve (manga-detail-table D2/D3/D6/D7/D8)") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    const std::string plugin = "enrichtest";
    const std::string slug = "mat-1";
    seed_saved(store, plugin, slug);

    SUBCASE("materialize persists resolved fields; source columns untouched") {
        const std::string resolved =
            R"({"ok":true,"data":{"name":"Resolved Name","description":"The description","synopsis":"ignored",
                "authors":["A1","A2"],"artists":["Art"],"tags":["T1","T2"],"title":"unused"}})";
        materialize_detail(store, db, plugin, slug, resolved);
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->name == "Resolved Name");
        CHECK(row->synopsis_display == "The description"); // description wins over synopsis
        CHECK(row->authors == R"(["A1","A2"])");
        CHECK(row->artists == R"(["Art"])");
        CHECK(row->tags == R"(["T1","T2"])");
        CHECK(row->synopsis.empty()); // D4: sync alone writes the source column
    }

    SUBCASE("malformed envelope is a quiet no-op") {
        materialize_detail(store, db, plugin, slug, "not json at all");
        materialize_detail(store, db, plugin, slug, R"({"ok":true})"); // no data object
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->synopsis_display.empty());
    }

    SUBCASE("row serve: cached flag, no enrichment without title_meta, D4 fallback") {
        const std::string resolved = R"({"ok":true,"data":{"name":"R","tags":["x"]}})";
        store.saved_signal_update(plugin, slug, "src synopsis", R"(["Alt"])");
        materialize_detail(store, db, plugin, slug, resolved);
        const std::string body = saved_detail_response(store, db, plugin, slug);
        CHECK(has(body, R"("cached":true)")); // D8
        CHECK(has(body, R"("name":"R")"));
        CHECK(has(body, R"("tags":["x"])"));
        CHECK(has(body, R"("description":"src synopsis")")); // D4: display falls back to source
        CHECK(has(body, R"("alt_titles":["Alt"])"));
        CHECK(has(body, R"("status":"ongoing")")); // stats echo (schema default)
        CHECK_FALSE(has(body, "enrichment")); // nothing fetched yet

        // D7: title_meta overlay rides along
        store.title_meta_put_fetched(plugin, slug, R"([{"source":"m","fetched_at":7,"data":{}}])", 7);
        store.title_meta_put_chosen(plugin, slug, R"({"cover":"https://c/z.webp"})");
        const std::string overlaid = saved_detail_response(store, db, plugin, slug);
        CHECK(has(overlaid, R"("chosen_cover":"https://c/z.webp")"));
        CHECK(has(overlaid, R"("fetched_at":7)"));
        CHECK(has(overlaid, R"("chosen")"));
    }

    SUBCASE("D6: reset detail drops the row serve; cover-only reset keeps it") {
        materialize_detail(store, db, plugin, slug, R"({"ok":true,"data":{"name":"R","tags":["x"]}})");
        store.title_meta_put_chosen(plugin, slug, R"({"cover":"https://c/z.webp"})");
        store.title_meta_reset_cover(plugin, slug);
        CHECK_FALSE(saved_detail_response(store, db, plugin, slug).empty()); // detail survives
        store.saved_detail_clear(plugin, slug);
        CHECK(saved_detail_response(store, db, plugin, slug).empty()); // nothing materialized left
    }

    SUBCASE("absent row and empty-detail rows do not serve") {
        CHECK(saved_detail_response(store, db, "nope", "x").empty());
        saved_detail_response(store, db, plugin, slug); // unsaved-but-present: quiet
        CHECK(saved_detail_response(store, db, plugin, slug).empty()); // nothing materialized yet
    }

    SUBCASE("apply_chosen stays read-only over saved rows (D2 purity, pairs with 5.2 guard)") {
        store.title_meta_put_fetched(plugin, slug, R"([{"source":"m","fetched_at":3,"data":{"title":"T"}}])", 3);
        store.title_meta_put_chosen(plugin, slug, R"({"synopsis":"chosen text"})");
        const std::string env = R"({"ok":true,"data":{"name":"N","description":"d"}})";
        const std::string out = apply_chosen(store, db, plugin, slug, env);
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->synopsis_display.empty()); // no write happened inside apply_chosen
        materialize_detail(store, db, plugin, slug, out);
        CHECK(store.saved_get(plugin, slug)->synopsis_display == "chosen text"); // chosen rides the display
    }
}

TEST_CASE("enrich: apply_read_rollup heals total_ch and counts done (row-serve path, f039549 fix)") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    seed_saved(store, "mangadex", "rollup-heal"); // total_ch 0, like rows saved before the fix

    const std::string env =
        R"({"ok":true,"data":[{"number":1,"id":"a1","pages":3},{"number":2,"id":"b2","pages":4}]})";
    // A row-served detail open runs ONLY this rollup (no sync): total must
    // heal from the chapters envelope. chapter-state D5: the read count comes
    // from the per-chapter bit0 rows -- progress keys alone no longer count;
    // the host stamps the bit at last-page (3.3) and bulk routes set it
    // directly, so the rollup only reads flags.
    apply_read_rollup(store, db, "mangadex", "rollup-heal", env);
    auto row = store.saved_get("mangadex", "rollup-heal");
    REQUIRE(row.has_value());
    CHECK_EQ(row->total_ch, 2);
    CHECK_EQ(row->read_ch, 0); // no bits yet
    CHECK((row->status_flags & 2) == 0);

    store.chapter_flags_apply("mangadex", "rollup-heal", {"a1"}, false, 1, 0); // host sets bit0
    apply_read_rollup(store, db, "mangadex", "rollup-heal", env);
    row = store.saved_get("mangadex", "rollup-heal");
    REQUIRE(row.has_value());
    CHECK_EQ(row->read_ch, 1);
    CHECK((row->status_flags & 2) == 0);

    store.chapter_flags_apply("mangadex", "rollup-heal", {"b2"}, false, 1, 0); // finish it
    apply_read_rollup(store, db, "mangadex", "rollup-heal", env);
    row = store.saved_get("mangadex", "rollup-heal");
    REQUIRE(row.has_value());
    CHECK_EQ(row->read_ch, 2);
    CHECK((row->status_flags & 2) != 0); // done
}

TEST_CASE("enrich: apply_read_rollup counts chapter rows when the chapters envelope is absent (NULL api_cache payload)") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    seed_saved(store, "mangadex", "rollup-noenv");

    const std::string env =
        R"({"ok":true,"data":[{"number":1,"id":"a1"},{"number":2,"id":"b2"}]})";
    apply_read_rollup(store, db, "mangadex", "rollup-noenv", env); // materialize rows
    store.chapter_flags_apply("mangadex", "rollup-noenv", {"a1", "b2"}, false, 1, 0);

    // A row-served open with a NULL/pruned api_cache chapters payload used to
    // hit the envelope guard and leave read_ch stale forever, so Done never
    // lit up even though every chapter row was flagged read.
    apply_read_rollup(store, db, "mangadex", "rollup-noenv", "");
    auto row = store.saved_get("mangadex", "rollup-noenv");
    REQUIRE(row.has_value());
    CHECK_EQ(row->total_ch, 2);
    CHECK_EQ(row->read_ch, 2);
    CHECK((row->status_flags & 2) != 0); // done
}

TEST_CASE("promote: save-time promotion materializes cached envelopes; no-cache is a no-op (library-promote-on-save)") {
    TempRoot root;
    Store store = store_at(root.path);
    BS::thread_pool<> db(1);
    const std::string plugin = "promotetest";
    const std::string slug = "promo-1";
    crow::json::wvalue p;
    p["id"] = slug;
    const std::string params = p.dump();
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    const std::string detail_env =
        R"({"ok":true,"data":{"name":"Promo Name","cover":"c.jpg","status":"ongoing","authors":["A1"],"artists":[],"tags":["T1"],"description":"The desc"}})";
    const std::string chs_env =
        R"({"ok":true,"data":[{"id":"c1","number":1,"title":"one","url":"u1","release_at":1700000000},{"id":"c2","number":2,"title":"two","url":"u2"}]})";
    store.api_cache_put(plugin, stale_key(plugin, "detail", params), params, detail_env, now, 3600);
    store.api_cache_put(plugin, stale_key(plugin, "chapters", params), params, chs_env, now, 3600);
    seed_saved(store, plugin, slug); // the save the route performed first

    SUBCASE("promotion fills detail + chapter rows with no plugin call") {
        http::api::promote_saved_cached(store, db, plugin, slug);
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->name == "Promo Name");
        CHECK(row->authors == R"(["A1"])");
        CHECK(row->synopsis_display == "The desc");
        CHECK_EQ(row->total_ch, 2);
        const auto chs = store.chapters_list(plugin, slug);
        REQUIRE(chs.size() == 2); // reading order
        CHECK(chs[0].chapter_id == "c1");
        CHECK(chs[0].release_at == "1700000000"); // numeric epoch -> integer text
        CHECK(chs[1].chapter_id == "c2");
        CHECK(chs[1].release_at.empty());
    }

    SUBCASE("idempotent: a second promotion changes nothing") {
        http::api::promote_saved_cached(store, db, plugin, slug);
        const auto before = store.saved_get(plugin, slug);
        http::api::promote_saved_cached(store, db, plugin, slug);
        const auto after = store.saved_get(plugin, slug);
        REQUIRE(before.has_value());
        REQUIRE(after.has_value());
        CHECK(before->authors == after->authors);
        CHECK(before->synopsis_display == after->synopsis_display);
        CHECK(before->updated_at == after->updated_at);
        CHECK(store.chapters_list(plugin, slug).size() == 2);
    }

    SUBCASE("name guard: slug/empty envelope name never lands over a real one (1.3)") {
        const std::string unnamed = R"({"ok":true,"data":{"authors":["A"],"description":"d"}})";
        store.api_cache_put(plugin, stale_key(plugin, "detail", params), params, unnamed, now, 3600);
        http::api::promote_saved_cached(store, db, plugin, slug);
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->name == "Name promo-1"); // seed_saved name survives
        CHECK(row->authors == R"(["A"])");
    }

    SUBCASE("nothing cached: silent no-op, lazy path intact") {
        const std::string other = "promo-cold";
        SavedManga cold;
        cold.plugin = plugin;
        cold.slug = other;
        cold.name = "Cold";
        store.save_manga(cold);
        http::api::promote_saved_cached(store, db, plugin, other);
        const auto row = store.saved_get(plugin, other);
        REQUIRE(row.has_value());
        CHECK(row->authors.empty());
        CHECK(store.chapters_list(plugin, other).empty());
    }
}
