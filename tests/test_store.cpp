#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <sqlite3.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <ctime>

#include "store/store.hpp"

// Dummy seed plugins (Asura/Flame/Reaper/Void) were removed from the store;
// this fixture inserts its own rows via raw SQL so tests stay self-contained.
// Wipe order: title then plugin (FK). Safe to re-run: deletes existing rows first.
static void seed_fixture(const char* path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path, &db) != SQLITE_OK) throw std::runtime_error("fixture: open failed");
    sqlite3_exec(db, "DELETE FROM title; DELETE FROM plugin;", nullptr, nullptr, nullptr);
    for (const char* name : {"Asura", "Flame"}) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db,
                "INSERT INTO plugin(name, plugin_id, version, capabilities, enabled, runtime) VALUES(?,'','','',1,'')",
                -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
        for (int n = 1; n <= 12; ++n) {
            st = nullptr;
            if (sqlite3_prepare_v2(db,
                    "INSERT INTO title(plugin_id, name) VALUES((SELECT id FROM plugin WHERE name=?), ?)",
                    -1, &st, nullptr) == SQLITE_OK) {
                const std::string title = std::string(name) + " Scans #" + std::to_string(n);
                sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
                sqlite3_bind_text(st, 2, title.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(st);
                sqlite3_finalize(st);
            }
        }
    }
    sqlite3_close(db);
}

struct TestStoreDb {
    std::string path = "test_store.db";
    TestStoreDb() {
        // start from a clean file so plugin/title counts are deterministic
        std::remove(path.c_str());
        { Store store(path); } // create schema
        seed_fixture(path.c_str());
    }
    ~TestStoreDb() { std::remove(path.c_str()); }
};

TEST_CASE("Store: fixture, plugins, search, count") {
    TestStoreDb fixture;
    Store store(fixture.path);

    SUBCASE("plugins lists 2 with 12 titles each") {
        auto plugins = store.plugins();
        REQUIRE(plugins.size() == 2);
        for (const auto& p : plugins) {
            CHECK(p.titles == 12);
        }
        CHECK(plugins[0].name == "Asura"); // order_by name
    }

    SUBCASE("count without filter") {
        CHECK(store.count("", "") == 24);
    }

    SUBCASE("count with plugin filter") {
        CHECK(store.count("Asura", "") == 12);
        CHECK(store.count("Nope", "") == 0);
    }

    SUBCASE("count with LIKE escaping: metacharacters stay literal") {
        CHECK(store.count("", "_") == 0);        // no literal underscore in seeds
        CHECK(store.count("", "%") == 0);        // % must not act as wildcard
        CHECK(store.count("", "Scans #3") == 2); // literal # matches (Asura+Flame)
        CHECK(store.count("", "Asura") == 12);   // plain substring still works
    }

    SUBCASE("search pagination") {
        auto page1 = store.search("", "", 12, 0);
        auto page2 = store.search("", "", 12, 12);
        REQUIRE(page1.size() == 12);
        REQUIRE(page2.size() == 12);
        CHECK(page1[0].id != page2[0].id);
        // ordered by name: page 2 starts where page 1 ended
        CHECK(page1[11].name < page2[0].name);
    }

    SUBCASE("search joins plugin name") {
        auto rows = store.search("Flame", "Scans #1", 10, 0);
        REQUIRE(rows.size() == 4); // #1, #10, #11, #12 all match '%Scans #1%'
        for (const auto& r : rows)
            CHECK(r.plugin == "Flame");
        auto exact = store.search("Flame", "Scans #1", 10, 0);
        CHECK(exact[0].name == "Flame Scans #1"); // order_by name puts #1 first
    }

    SUBCASE("plugin state round-trip and overwrite") {
        // hermetic: probe key unique per run, the db file persists between runs
        const auto probe = "probe_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        CHECK_FALSE(store.state_get("mangadex", probe).has_value());
        store.state_set("mangadex", probe, "r1");
        CHECK(store.state_get("mangadex", probe) == "r1");
        store.state_set("mangadex", probe, "r2"); // same PK -> update
        CHECK(store.state_get("mangadex", probe) == "r2");
        store.state_set("other", probe, "x"); // scoped per plugin
        CHECK(store.state_get("other", probe) == "x");
        CHECK(store.state_get("mangadex", probe) == "r2");
        { // fresh connection = what a host restart observes on disk
            Store reopened("test_store.db");
            CHECK(reopened.state_get("mangadex", probe) == "r2");
        }
    }

    SUBCASE("reader position round-trip, upsert, per-manga scope") {
        const auto slug = "pos_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        CHECK_FALSE(store.position_get("md", slug).has_value());
        store.position_set({"md", slug, "ch-uuid-1", 12.5, 3, "spread-h", "rtl", "fit-w", 0});
        const auto pos = store.position_get("md", slug);
        REQUIRE(pos.has_value());
        CHECK(pos->plugin == "md");
        CHECK(pos->slug == slug);
        CHECK(pos->chapter_id == "ch-uuid-1");
        CHECK(pos->chapter_number == 12.5); // fractional extras survive
        CHECK(pos->page == 3);
        CHECK(pos->mode == "spread-h");
        CHECK(pos->direction == "rtl");
        CHECK(pos->updated_at > 0); // store-stamped, caller value ignored

        // same (plugin, slug) replaces: the 2.2 route layers optional fields
        // over the stored row before calling this
        store.position_set({"md", slug, "ch-uuid-2", 13, 1, "", "", "", 0});
        const auto upd = store.position_get("md", slug);
        REQUIRE(upd.has_value());
        CHECK(upd->chapter_id == "ch-uuid-2");
        CHECK(upd->page == 1);
        CHECK(upd->mode.empty());
        CHECK(upd->direction.empty());

        store.position_set({"md", slug + "x", "c", 1, 1, "", "", "", 0}); // neighbor manga isolated
        CHECK(store.position_get("md", slug)->chapter_id == "ch-uuid-2");
        CHECK_FALSE(store.position_get("other", slug).has_value()); // plugin scope too

        { // fresh connection = what a host restart observes on disk
            Store reopened("test_store.db");
            const auto disk = reopened.position_get("md", slug);
            REQUIRE(disk.has_value());
            CHECK(disk->chapter_id == "ch-uuid-2");
        }
    }

    SUBCASE("plugin enabled flag persists") {
        auto rows = store.plugins();
        REQUIRE(!rows.empty());
        auto& first = rows[0];
        CHECK(first.enabled); // default on, also for pre-migration rows
        store.set_plugin_enabled(first.id, false);
        for (const auto& p : store.plugins()) {
            if (p.id == first.id) {
                CHECK_FALSE(p.enabled);
            }
        }
        store.set_plugin_enabled(first.id, true); // restore for other subcases/runs
        for (const auto& p : store.plugins()) {
            if (p.id == first.id) {
                CHECK(p.enabled);
            }
        }
    }
}

TEST_CASE("Store: saved_manga save/unsave/list (library-save 1.2)") {
    Store store("test_store.db");

    SUBCASE("save upserts: re-save refreshes name/cover, rows never duplicate") {
        store.unsave_manga("md", "abc-slug-1");
        store.save_manga({"md", "abc-slug-1", "Name One", "https://cover/one.jpg", 0});
        store.save_manga({"md", "abc-slug-1", "Name One v2", "https://cover/one-v2.jpg", 0});
        auto list = store.saved_list();
        int hits = 0;
        for (const auto& s : list) if (s.plugin == "md" && s.slug == "abc-slug-1") ++hits;
        CHECK(hits == 1);
        auto got = store.saved_get("md", "abc-slug-1");
        REQUIRE(got.has_value());
        CHECK(got->name == "Name One v2");
        CHECK(got->cover == "https://cover/one-v2.jpg");
    }

    SUBCASE("list orders by added_at desc (newest first)") {
        store.unsave_manga("md", "abc-slug-old");
        store.unsave_manga("md", "abc-slug-new");
        store.save_manga({"md", "abc-slug-old", "Old", "", 0});
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // force strictly newer added_at on the second row
        store.unsave_manga("md", "abc-slug-new");
        store.save_manga({"md", "abc-slug-new", "New", "", 0});
        auto list = store.saved_list();
        int idx_old = -1, idx_new = -1;
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (list[i].slug == "abc-slug-old") idx_old = static_cast<int>(i);
            if (list[i].slug == "abc-slug-new") idx_new = static_cast<int>(i);
        }
        REQUIRE(idx_old >= 0);
        REQUIRE(idx_new >= 0);
        CHECK(idx_new < idx_old);
    }

    SUBCASE("unsave deletes; saved_get returns nullopt") {
        store.save_manga({"md", "abc-slug-del", "Del", "", 0});
        REQUIRE(store.saved_get("md", "abc-slug-del").has_value());
        store.unsave_manga("md", "abc-slug-del");
        CHECK_FALSE(store.saved_get("md", "abc-slug-del").has_value());
        store.unsave_manga("md", "abc-slug-del"); // idempotent remove
    }
}

TEST_CASE("Store: dup_group clustering (library-dedup 4.1)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string p1 = "md_" + run;
    const std::string p2 = "at_" + run;
    const std::string s1 = "dup-a-" + run, s2 = "dup-b-" + run, s3 = "dup-c-" + run;

    // same work: same title tokens across plugins, slightly different synopses
    store.save_manga({p1, s1, "Re:Zero Starting Life in Another World", "", 0});
    store.save_manga({p2, s2, "Re Zero Starting Life in Another World", "", 0});
    store.saved_signal_update(p1, s1, "Subaru Natsuki gets isekai'd into a fantasy world", "[\"Re:Zero\", \"Re Zero\"]");
    store.saved_signal_update(p2, s2, "Subaru Natsuki is reincarnated into a fantasy world", "[\"Re Zero\"]");
    // unrelated work with distinct tokens
    store.save_manga({p1, s3, "One Piece", "", 0});
    store.saved_signal_update(p1, s3, "Pirates explore the Grand Line", "[\"One Piece\"]");
    store.recompute_dup_groups();

    const auto a = store.saved_get(p1, s1);
    const auto b = store.saved_get(p2, s2);
    const auto c = store.saved_get(p1, s3);
    REQUIRE(a.has_value()); REQUIRE(b.has_value()); REQUIRE(c.has_value());
    // pair shares all title tokens -> same group; c does not
    CHECK(a->dup_group != 0);
    CHECK(a->dup_group == b->dup_group);
    CHECK(c->dup_group == 0);

    // unsave one member -> remaining member becomes unique
    store.unsave_manga(p2, s2);
    store.recompute_dup_groups();
    CHECK(store.saved_get(p1, s1)->dup_group == 0);
}

TEST_CASE("Store: saved manga stats roundtrip (library-grid-stats 1.3/1.4)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md";
    const std::string slug = "stats-slug-" + run;

    SUBCASE("fresh save carries schema defaults") {
        store.save_manga({plugin, slug, "Stats", "", 0});
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->status == "ongoing");
        CHECK(row->total_ch == 0);
        CHECK(row->read_ch == 0);
        CHECK(row->has_update == 0);
        CHECK(row->last_read_at == 0);
        CHECK(row->updated_at == 0);
    }

    SUBCASE("stats_update / read_rollup / clear_update lifecycle") {
        store.save_manga({plugin, slug, "Stats", "", 0});
        store.saved_stats_update(plugin, slug, "complete", 12);
        auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->status == "complete");
        CHECK(row->total_ch == 12);
        CHECK(row->has_update == 1); // 12 > 0 -> growth marks New
        CHECK(row->updated_at > 0);

        store.saved_read_rollup(plugin, slug, 7, 0); // ts 0 = leave last_read alone
        row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->read_ch == 7);
        CHECK(row->last_read_at == 0);

        const std::int64_t ts = 1'700'000'000;
        store.saved_read_rollup(plugin, slug, 8, ts);
        row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->read_ch == 8);
        CHECK(row->last_read_at == ts);

        store.saved_clear_update(plugin, slug); // reader touch: clears flag, stamps now
        row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->has_update == 0);
        CHECK(row->last_read_at > ts);

        const auto before = row->last_read_at;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        store.saved_clear_update(plugin, slug); // flag-less touch still stamps
        row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->last_read_at > before);
    }

    SUBCASE("saved_list carries stats; absent rows are no-ops") {
        store.save_manga({plugin, slug, "Stats", "", 0});
        store.saved_stats_update(plugin, slug, "hiatus", 3);
        const auto list = store.saved_list();
        bool seen = false;
        for (const auto& m : list)
            if (m.slug == slug) {
                seen = true;
                CHECK(m.status == "hiatus");
                CHECK(m.total_ch == 3);
                CHECK(m.has_update == 1);
            }
        CHECK(seen);

        const std::string absent = "stats-absent-" + run;
        store.saved_stats_update(plugin, absent, "x", 1); // quiet no-ops
        store.saved_read_rollup(plugin, absent, 1, 1);
        store.saved_clear_update(plugin, absent);
    }
}

TEST_CASE("Store: api_cache JSONB round-trip (write via jsonb(), read via json())") {
    Store store("test_store.db"); // same file as above; unique keys per run
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md_" + run;
    const std::string key = "hash_" + run;
    const std::string payload = R"({"ok":true,"data":[{"id":"a"}]})";

    CHECK_FALSE(store.api_cache_get(plugin, key).has_value());

    store.api_cache_put(plugin, key, R"({"q":"x"})", payload, 1000, 3600);
    const auto row = store.api_cache_get(plugin, key);
    REQUIRE(row.has_value());
    CHECK(row->payload == payload); // json() round-trip preserves content
    CHECK(row->params_json == R"({"q":"x"})");
    CHECK(row->fetched_at == 1000);
    CHECK(row->ttl_s == 3600);

    // upsert on same PK replaces, never duplicates
    store.api_cache_put(plugin, key, R"({"q":"y"})", R"({"ok":true,"data":[]})", 2000, 0);
    const auto updated = store.api_cache_get(plugin, key);
    REQUIRE(updated.has_value());
    CHECK(updated->payload == R"({"ok":true,"data":[]})");
    CHECK(updated->fetched_at == 2000);
    CHECK(updated->ttl_s == 0);

    // malformed JSON is rejected by jsonb() at write time; existing row preserved
    store.api_cache_put(plugin, key, R"({"q":"y"})", R"({"unterminated)", 3000, 99);
    const auto kept = store.api_cache_get(plugin, key);
    REQUIRE(kept.has_value());
    CHECK(kept->payload == R"({"ok":true,"data":[]})"); // not overwritten by garbage
    CHECK(kept->fetched_at == 2000);

    // plugin isolation
    const auto other = store.api_cache_get("other_" + run, key);
    CHECK_FALSE(other.has_value());
}

TEST_CASE("Store: api_cache NULL/empty payload is a miss, not a fresh hit") {
    Store store("test_store.db"); // file persists; unique keys per run
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "mdnull_" + run;
    const std::string key = "isnull_" + run;

    // plant a row shaped like the legacy NULL-payload rows directly via raw SQL
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open("test_store.db", &db) == SQLITE_OK);
    sqlite3_stmt* st = nullptr;
    const char* sql = "INSERT INTO api_cache(plugin_id, query_hash, params_json, fetched_at, ttl_s, payload) VALUES(?,?,?,?,?,NULL)";
    REQUIRE(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, R"({"q":"x"})", -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 4, 1000);
    sqlite3_bind_int64(st, 5, 86400);
    REQUIRE(sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    sqlite3_close(db);

    // fresh look-aside must miss despite a row existing
    CHECK_FALSE(store.api_cache_get(plugin, key).has_value());
}

TEST_CASE("Store: title_meta round-trip and corrupt-row tolerance (enrichment 2.1-2.3)") {
    Store store("test_store.db");
    const std::string plugin = "enrich_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string slug = "slug-" + plugin;

    CHECK_FALSE(store.title_meta_get(plugin, slug).has_value());

    store.title_meta_put_fetched(plugin, slug, R"([{"source":"mangadex","fetched_at":1000,"data":{"title":"A"}}])", 1000);
    {
        const auto row = store.title_meta_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->fetched_at == 1000);
        CHECK(row->chosen == "{}");
    }

    store.title_meta_put_chosen(plugin, slug, R"({"title":"B","tags_add":["Isekai"],"tags_hidden":["Ecchi"],"cover":"https://x/c.jpg"})");
    {
        const auto row = store.title_meta_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->chosen.find("\"title\":\"B\"") != std::string::npos);
        CHECK(row->chosen.find("tags_hidden") != std::string::npos);
        CHECK(row->fetched.find("mangadex") != std::string::npos);
    }

    store.title_meta_reset_cover(plugin, slug);
    {
        const auto row = store.title_meta_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->chosen.find("cover") == std::string::npos);
        CHECK(row->chosen.find("\"title\":\"B\"") != std::string::npos);
        CHECK(row->fetched.find("mangadex") != std::string::npos); // fetched kept
    }

    store.title_meta_reset_detail(plugin, slug);
    {
        const auto row = store.title_meta_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->chosen.find("\"title\"") == std::string::npos);
        CHECK(row->chosen.find("tags_add") == std::string::npos);
        CHECK(row->chosen.find("tags_hidden") == std::string::npos);
        CHECK(row->fetched.find("mangadex") != std::string::npos); // fetched kept across detail reset
    }

    // corrupt fetched row -> read as absent, never crash
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open("test_store.db", &db) == SQLITE_OK);
    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, "UPDATE title_meta SET fetched = 'not json{{' WHERE plugin = ? AND slug = ?", -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
    REQUIRE(sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    sqlite3_close(db);
    CHECK_FALSE(store.title_meta_get(plugin, slug).has_value());

    // corrupt chosen row -> read as absent too
    REQUIRE(sqlite3_open("test_store.db", &db) == SQLITE_OK);
    REQUIRE(sqlite3_prepare_v2(db, "UPDATE title_meta SET fetched = '[]', chosen = '{' WHERE plugin = ? AND slug = ?", -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, plugin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, slug.c_str(), -1, SQLITE_TRANSIENT);
    REQUIRE(sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);
    sqlite3_close(db);
    CHECK_FALSE(store.title_meta_get(plugin, slug).has_value());
}

TEST_CASE("Store: update_attempts upsert/backoff/reset (auto-update 2.2)") {
    TestStoreDb fixture;
    Store store(fixture.path);

    auto save = [&](const std::string& plugin, const std::string& slug, int read_ch) {
        SavedManga m;
        m.plugin = plugin; m.slug = slug; m.name = slug; m.cover = ""; m.read_ch = read_ch;
        store.save_manga(m);
    };
    save("Asura", "m1", 0);

    SUBCASE("first error: consecutive_errors=1, backoff ~now+2h") {
        store.upsert_update_attempt("Asura", "m1", "error", "boom");
        auto a = store.update_attempt_get("Asura", "m1");
        REQUIRE(a.has_value());
        CHECK(a->consecutive_errors == 1);
        CHECK(a->outcome == "error");
        CHECK(a->error == "boom");
        CHECK(a->backoff_until > 0);
    }
    SUBCASE("success resets counter and backoff") {
        store.upsert_update_attempt("Asura", "m1", "error", "boom");
        store.upsert_update_attempt("Asura", "m1", "error", "boom");
        store.upsert_update_attempt("Asura", "m1", "ok", "");
        auto a = store.update_attempt_get("Asura", "m1");
        REQUIRE(a.has_value());
        CHECK(a->consecutive_errors == 0);
        CHECK(a->backoff_until == 0);
        CHECK(a->outcome == "ok");
    }
    SUBCASE("not attempted -> nullopt") {
        CHECK_FALSE(store.update_attempt_get("Asura", "never").has_value());
    }
}

TEST_CASE("Store: update_candidates per-plugin cap, ordering, backoff exclusion (auto-update 2.2)") {
    TestStoreDb fixture;
    Store store(fixture.path);
    auto now = static_cast<std::int64_t>(std::time(nullptr));

    auto save = [&](const std::string& plugin, const std::string& slug, int read_ch) {
        SavedManga m;
        m.plugin = plugin; m.slug = slug; m.name = slug; m.cover = ""; m.read_ch = read_ch;
        store.save_manga(m);
    };
    for (int i = 0; i < 30; ++i) save("Asura", "a" + std::to_string(i), i % 2); // all <=1
    for (int i = 0; i < 30; ++i) save("Flame", "f" + std::to_string(i), 0);
    save("Flame", "f_wellread", 5); // excluded

    SUBCASE("cap 10 per plugin: 2 plugins -> 20, well-read excluded") {
        auto c = store.update_candidates(now, 10);
        int asura = 0, flame = 0;
        for (const auto& a : c) {
            if (a.plugin == "Asura") ++asura;
            if (a.plugin == "Flame") ++flame;
            CHECK(a.slug != "f_wellread");
        }
        CHECK(asura == 10);
        CHECK(flame == 10);
    }
    SUBCASE("never-attempted first; oldest last_attempt_at next") {
        // stamp attempts on two Asura titles
        store.upsert_update_attempt("Asura", "a0", "ok", "");
        store.upsert_update_attempt("Asura", "a1", "ok", "");
        auto c = store.update_candidates(now + 100000, 2); // far future: all not backed off
        int asura_seen = 0;
        for (const auto& a : c)
            if (a.plugin == "Asura") { ++asura_seen; CHECK(a.slug != "a0"); CHECK(a.slug != "a1"); }
        CHECK(asura_seen == 2);
    }
    SUBCASE("backed-off title excluded") {
        store.upsert_update_attempt("Asura", "a0", "error", "boom"); // backoff_until = now + 2h
        auto c = store.update_candidates(now, 30);
        for (const auto& a : c)
            CHECK(a.slug != "a0");
    }
}
