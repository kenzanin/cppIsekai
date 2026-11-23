#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <sqlite3.h>

#include <atomic>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <fstream>
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

    SUBCASE("pin toggle: set/clear persists, re-save preserves (library-pin)") {
        store.unsave_manga("md", "pin-slug");
        store.save_manga({"md", "pin-slug", "Pinned One", "", 0});
        CHECK_FALSE(store.saved_get("md", "pin-slug")->pinned);
        store.saved_set_pinned("md", "pin-slug", true);
        CHECK(store.saved_get("md", "pin-slug")->pinned);
        store.save_manga({"md", "pin-slug", "Pinned One v2", "", 0}); // re-save keeps the pin
        CHECK(store.saved_get("md", "pin-slug")->pinned);
        store.saved_set_pinned("md", "pin-slug", false);
        CHECK_FALSE(store.saved_get("md", "pin-slug")->pinned);
        store.saved_set_pinned("md", "pin-slug-absent", true); // no row: silent no-op
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
        CHECK(row->status_flags == 0); // fresh save: no bits
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
        CHECK((row->status_flags & 1) != 0); // 12 > 0 -> growth marks New (bit0)
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

        store.saved_clear_update(plugin, slug); // reader touch: clears bit0, stamps now
        row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK((row->status_flags & 1) == 0);
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
                CHECK((m.status_flags & 1) != 0); // 3 > 0 -> growth marks New (bit0)
            }
        CHECK(seen);

        const std::string absent = "stats-absent-" + run;
        store.saved_stats_update(plugin, absent, "x", 1); // quiet no-ops
        store.saved_read_rollup(plugin, absent, 1, 1);
        store.saved_clear_update(plugin, absent);
        store.saved_dim_update(plugin, absent, true); // unsaved row: toggle stays a no-op
    }

    SUBCASE("status_flags lifecycle: new -> done -> category -> dim") {
        const unsigned FRESH = 0; // fresh save: no state bits yet
        store.save_manga({plugin, slug, "Life", "", 0});
        REQUIRE(store.saved_get(plugin, slug)->status_flags == FRESH); // fresh save: no state bits

        // first stat write: the stored status ("ongoing") becomes category bit2,
        // growth beyond the stored total raises bit0
        store.saved_stats_update(plugin, slug, "", 5);
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 4 | 1);

        store.saved_read_rollup(plugin, slug, 5, 0); // read_ch >= total -> bit1, bit0 drops
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 4 | 2);

        const std::pair<const char*, unsigned> cats[] = {
            {"ongoing", 4}, {"hiatus", 8},
            {"canceled", 16}, {"cancelled", 16},
            {"completed", 32}, {"weird", 0},
        };
        for (const auto& [st, mask] : cats) {
            store.saved_stats_update(plugin, slug, st, 5); // flat count; read_ch == total_ch -> done
            const auto row = store.saved_get(plugin, slug);
            REQUIRE(row.has_value());
            CHECK(row->status == st); // source status stays verbatim
            // exactly the category bit (+ bit1: read_ch >= total_ch) and nothing else
            CHECK_EQ(static_cast<unsigned>(row->status_flags), mask | 2);
        }

        store.saved_stats_update(plugin, slug, "completed", 5); // category bit5 back
        store.saved_dim_update(plugin, slug, true); // bit6 sets alone (category + done ride through)
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 2 | 64);
        store.saved_dim_update(plugin, slug, false);
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 2);

        // stat writers preserve bit6 through the composition
        store.saved_dim_update(plugin, slug, true);
        store.saved_read_rollup(plugin, slug, 1, 0); // unread again: not done
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 64);
        store.saved_stats_update(plugin, slug, "", 6); // growth: bit0 up, dim kept
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 1 | 64);
        store.saved_read_rollup(plugin, slug, 6, 0); // done: bit1 up, bit0 drops, dim kept
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 2 | 64);

        // reader touch clears bit0 only (category + dim survive)
        store.saved_read_rollup(plugin, slug, 1, 0); // not done again
        store.saved_stats_update(plugin, slug, "", 7); // growth: bit0 up
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 1 | 64);
        store.saved_clear_update(plugin, slug);
        CHECK_EQ(store.saved_get(plugin, slug)->status_flags, 32 | 64);

        // negative total = "envelope absent": the stored count survives
        // (regression -- a sync with a missing chapters cache zeroed
        // total_ch and done = read_ch >= total_ch never fired)
        store.saved_stats_update(plugin, slug, "", -1);
        CHECK_EQ(store.saved_get(plugin, slug)->total_ch, 7);
    }

    SUBCASE("dim list returns exactly the rows with bit6 set") {
        const std::string dplugin = "dim_" + run; // own plugin: the db file is shared across runs
        const std::string d1 = "dim-a-" + run, d2 = "dim-b-" + run, d3 = "dim-c-" + run;
        for (const auto& d : {d1, d2, d3}) {
            store.save_manga({dplugin, d, d, "", 0});
        }
        store.saved_dim_update(dplugin, d1, true);
        store.saved_dim_update(dplugin, d2, true);
        const auto dims = store.saved_dim_list();
        std::vector<std::string> mine;
        for (const auto& [p, sl] : dims) {
            if (p == dplugin) {
                mine.push_back(sl);
            }
        }
        REQUIRE(mine.size() == 2); // d3 untouched
        bool saw_d1 = false;
        bool saw_d2 = false;
        for (const auto& sl : mine) {
            saw_d1 = saw_d1 || sl == d1;
            saw_d2 = saw_d2 || sl == d2;
            CHECK(sl != d3);
        }
        CHECK(saw_d1);
        CHECK(saw_d2);
        // unsaved manga: the toggle is a no-op, no row appears
        store.saved_dim_update(dplugin, "not-saved-" + run, true);
        CHECK(store.saved_get(dplugin, "not-saved-" + run) == std::nullopt);
    }
}

TEST_CASE("Store: legacy saved_manga migrates has_update into status_flags (design D5)") {
    const std::string path = "test_store_legacy.db";
    std::remove(path.c_str());
    { // a pre-status_flags library: has_update present, status_flags missing
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
        sqlite3_exec(db,
                     "CREATE TABLE saved_manga(plugin TEXT NOT NULL, slug TEXT NOT NULL, name TEXT NOT NULL, "
                     "cover TEXT NOT NULL, added_at INTEGER NOT NULL, status TEXT NOT NULL DEFAULT 'ongoing', "
                     "total_ch INTEGER NOT NULL DEFAULT 0, read_ch INTEGER NOT NULL DEFAULT 0, "
                     "has_update INTEGER NOT NULL DEFAULT 0, last_read_at INTEGER NOT NULL DEFAULT 0, "
                     "updated_at INTEGER NOT NULL DEFAULT 0, synopsis TEXT NOT NULL DEFAULT '', "
                     "alt_titles TEXT NOT NULL DEFAULT '', dup_group INTEGER NOT NULL DEFAULT 0, "
                     "PRIMARY KEY(plugin, slug))",
                     nullptr, nullptr, nullptr);
        // three rows: new (has_update), done (read_ch == total_ch), done+update
        sqlite3_exec(db,
                     "INSERT INTO saved_manga(plugin, slug, name, cover, added_at, status, total_ch, read_ch, has_update) "
                     "VALUES('md','new','New','',1,'ongoing',10,3,1), "
                     "('md','done','Done','',2,'completed',10,10,0), "
                     "('md','done-new','DoneNew','',3,'hiatus',10,10,1)",
                     nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }
    { Store store(path); } // the migration runs here
    { Store store(path); } // second boot: the migration is idempotent

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(db, "PRAGMA table_info(saved_manga)", -1, &st, nullptr) == SQLITE_OK);
    bool has_update = false;
    bool has_status_flags = false;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        has_update = has_update || std::string(name) == "has_update";
        has_status_flags = has_status_flags || std::string(name) == "status_flags";
    }
    sqlite3_finalize(st);
    CHECK_FALSE(has_update); // the column is gone
    CHECK(has_status_flags); // and the bitfield took its place

    auto flags_of = [&](const char* slug) {
        sqlite3_stmt* q = nullptr;
        REQUIRE(sqlite3_prepare_v2(db, "SELECT status_flags FROM saved_manga WHERE slug = ?", -1, &q, nullptr) ==
                SQLITE_OK);
        sqlite3_bind_text(q, 1, slug, -1, SQLITE_STATIC);
        int flags = -1;
        if (sqlite3_step(q) == SQLITE_ROW) {
            flags = sqlite3_column_int(q, 0);
        }
        sqlite3_finalize(q);
        return flags;
    };
    CHECK_EQ(flags_of("new"), 1 | 4);      // has_update -> bit0, "ongoing" -> bit2
    CHECK_EQ(flags_of("done"), 2 | 32);    // read_ch >= total_ch -> bit1, "completed" -> bit5
    CHECK_EQ(flags_of("done-new"), 2 | 8); // done wins over the update flag, "hiatus" -> bit3
    sqlite3_close(db);
    std::remove(path.c_str());
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
    SUBCASE("disabled plugin skipped (plugin-lifecycle 1.1)") {
        store.upsert_plugin("Asura", "Asura Scans", "1.0", "", "lua");
        std::int64_t wid = 0;
        for (const auto& p : store.plugins())
            if (p.plugin_id == "Asura")
                wid = p.id;
        REQUIRE(wid != 0);
        store.set_plugin_enabled(wid, false);
        for (const auto& a : store.update_candidates(now, 30))
            CHECK(a.plugin != "Asura");
        store.set_plugin_enabled(wid, true);
        int asura = 0;
        for (const auto& a : store.update_candidates(now, 30))
            if (a.plugin == "Asura")
                ++asura;
        CHECK(asura == 30);
    }
}

TEST_CASE("Store: plugin_prune_absent cascade (plugin-lifecycle 2.1)") {
    TestStoreDb fixture;
    Store store(fixture.path);

    store.upsert_plugin("Asura", "Asura Scans", "1.0", "search", "lua");
    store.upsert_plugin("Flame", "Flame Scans", "1.0", "search", "js");

    SavedManga m;
    m.plugin = "Asura"; m.slug = "m1"; m.name = "m1"; m.cover = ""; m.read_ch = 1;
    store.save_manga(m);
    store.state_set("Asura", "host/pos", "1");
    store.upsert_update_attempt("Asura", "m1", "ok", "");
    store.title_meta_put_fetched("Asura", "m1", "[]", 1);
    store.api_cache_put("Asura", "k1", "{}", "payload", 1, 60);
    ReaderPosition r;
    r.plugin = "Asura"; r.slug = "m1"; r.chapter_id = "c1"; r.chapter_number = 1; r.page = 1;
    store.position_set(r);

    const auto removed = store.plugin_prune_absent({"Flame"});
    REQUIRE(removed.size() == 1);
    CHECK(removed[0] == "Asura");

    // every trace gone, the survivor untouched, re-run is a no-op
    CHECK_FALSE(store.state_get("Asura", "host/pos").has_value());
    CHECK_FALSE(store.update_attempt_get("Asura", "m1").has_value());
    CHECK_FALSE(store.title_meta_get("Asura", "m1").has_value());
    CHECK_FALSE(store.api_cache_get("Asura", "k1").has_value());
    CHECK_FALSE(store.position_get("Asura", "m1").has_value());
    for (const auto& s : store.saved_list())
        CHECK(s.plugin != "Asura");
    for (const auto& p : store.plugins())
        CHECK(p.plugin_id != "Asura");
    CHECK(store.plugin_enabled("Flame"));
    CHECK(store.plugin_prune_absent({"Asura", "Flame"}).empty());
}

TEST_CASE("Store: title_meta_get_many batched overlay read (hot-path-perf 1.3)") {
    seed_fixture("test_store_meta_many.db");
    Store store("test_store_meta_many.db");
    const auto run = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string plugin = "Asura";
    const std::string s1 = "many-a-" + std::to_string(run);
    const std::string s2 = "many-b-" + std::to_string(run);

    store.title_meta_put_fetched(plugin, s1, R"([{"source":"m","fetched_at":1,"data":{}}])", 1);
    store.title_meta_put_chosen(plugin, s1, R"({"title":"T1","cover":"https://x/1.jpg"})");
    store.title_meta_put_fetched(plugin, s2, "[]", 2);
    store.title_meta_put_chosen(plugin, s2, R"({"title":"T2"})");

    const auto rows = store.title_meta_get_many({{plugin, s1}, {plugin, s2}, {"nope", "missing"}, {plugin, "absent"}});
    REQUIRE(rows.size() == 2); // absent pairs skipped
    REQUIRE(rows.count({plugin, s1}) == 1);
    CHECK(rows.at({plugin, s1}).fetched == R"([{"source":"m","fetched_at":1,"data":{}}])");
    CHECK(rows.at({plugin, s1}).chosen == R"({"title":"T1","cover":"https://x/1.jpg"})");
    CHECK(rows.at({plugin, s2}).chosen == R"({"title":"T2"})");
    // parity with the single-row getter
    const auto single = store.title_meta_get(plugin, s1);
    REQUIRE(single.has_value());
    CHECK(rows.at({plugin, s1}).fetched_at == single->fetched_at);
    CHECK(rows.at({plugin, s1}).chosen == single->chosen);
    CHECK(rows.at({plugin, s1}).fetched == single->fetched);
    CHECK(store.title_meta_get_many({}).empty());
}

TEST_CASE("Store: cache_prune_expired age-based reaper (hot-path-perf 3.3)") {
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Store store("test_store_cache_prune_" + run + ".db");
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    // payload must be valid JSON: api_cache_put writes through jsonb(?), which
    // rejects anything else and silently skips the row.
    // expired beyond the 7d grace / fresh / long-ttl inside the grace window
    store.api_cache_put("prune-plugin", "expired", "{}", "{}", now - 100 * 86400, 3600);
    store.api_cache_put("prune-plugin", "fresh", "{}", "{}", now, 3600);
    store.api_cache_put("prune-plugin", "longttl", "{}", "{}", now - 20 * 86400, 30 * 86400);
    REQUIRE(store.api_cache_get("prune-plugin", "expired").has_value());

    const auto n = store.cache_prune_expired();
    CHECK(n == 1); // only the expired row
    CHECK_FALSE(store.api_cache_get("prune-plugin", "expired").has_value());
    CHECK(store.api_cache_get("prune-plugin", "fresh").has_value());
    CHECK(store.api_cache_get("prune-plugin", "longttl").has_value());
}

TEST_CASE("Store: materialized detail columns (manga-detail-table 1.4/2.1/3.2)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md";

    SUBCASE("saved_detail_put writes display columns; source signals untouched") {
        const std::string slug = "mat-a-" + run;
        store.save_manga({plugin, slug, "Raw", "", 0});
        store.saved_signal_update(plugin, slug, "source synopsis", R"(["Alt One"])");
        CHECK(store.saved_detail_put({.plugin = plugin, .slug = slug, .name = "Resolved", .authors = R"(["Author A"])",
                                     .artists = R"(["Artist B"])", .tags = R"(["Tag 1","Tag 2"])",
                                     .synopsis_display = "display text", .cover = "https://x/c.png"}));
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->name == "Resolved");
        CHECK(row->authors == R"(["Author A"])");
        CHECK(row->artists == R"(["Artist B"])");
        CHECK(row->tags == R"(["Tag 1","Tag 2"])");
        CHECK(row->synopsis_display == "display text");
        CHECK(row->synopsis == "source synopsis"); // D4: source column owned by sync only
        CHECK(row->alt_titles == R"(["Alt One"])"); // source verbatim
    }

    SUBCASE("re-save preserves materialized columns; name refresh allowed") {
        const std::string slug = "mat-b-" + run;
        store.save_manga({plugin, slug, "Raw", "", 0});
        REQUIRE(store.saved_detail_put({.plugin = plugin, .slug = slug, .name = "Resolved", .authors = R"(["A"])",
                                        .tags = R"(["t"])", .synopsis_display = "d"}));
        store.save_manga({plugin, slug, "Re-saved", "", 0}); // plain re-save
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->name == "Re-saved"); // re-save refreshes identity, not detail
        CHECK(row->authors == R"(["A"])");
        CHECK(row->synopsis_display == "d");
    }

    SUBCASE("placeholder name never clobbers a stored one (D9 guard at the store seam)") {
        const std::string slug = "mat-c-" + run;
        store.save_manga({plugin, slug, "Real Name", "", 0});
        CHECK(store.saved_detail_put({.plugin = plugin, .slug = slug, .authors = R"(["A"])"})); // columns land, name kept
        CHECK(store.saved_get(plugin, slug)->name == "Real Name");
        CHECK(store.saved_detail_put({.plugin = plugin, .slug = slug, .name = slug, .synopsis_display = "x"})); // slug == name kept
        CHECK(store.saved_get(plugin, slug)->name == "Real Name");
        CHECK(store.saved_detail_put({.plugin = plugin, .slug = slug, .name = "Better"})); // real name lands
        CHECK(store.saved_get(plugin, slug)->name == "Better");
    }

    SUBCASE("saved_detail_clear wipes only the display columns") {
        const std::string slug = "mat-d-" + run;
        store.save_manga({plugin, slug, "Raw", "", 0});
        store.saved_signal_update(plugin, slug, "src", R"(["Alt"])");
        store.saved_stats_update(plugin, slug, "ongoing", 9);
        REQUIRE(store.saved_detail_put({.plugin = plugin, .slug = slug, .name = "R", .authors = R"(["A"])",
                                        .tags = R"(["t"])", .synopsis_display = "d"}));
        store.saved_detail_clear(plugin, slug);
        const auto row = store.saved_get(plugin, slug);
        REQUIRE(row.has_value());
        CHECK(row->authors.empty());
        CHECK(row->artists.empty());
        CHECK(row->tags.empty());
        CHECK(row->synopsis_display.empty());
        CHECK(row->name == "R"); // resolved name survives the display wipe
        CHECK(row->synopsis == "src"); // source signals survive the clear
        CHECK(row->alt_titles == R"(["Alt"])");
        CHECK(row->total_ch == 9); // stats survive
        store.saved_detail_clear("nope", "x"); // absent row no-op
        CHECK_FALSE(store.saved_detail_put({.plugin = "nope", .slug = "x", .name = "N", .authors = "[]", .artists = "[]",
                                            .tags = "[]"})); // absent row no-op
    }

    SUBCASE("saved_has_detail gate and recompute coexistence") {
        const std::string slug = "mat-e-" + run;
        store.save_manga({plugin, slug, "Raw", "", 0});
        CHECK_FALSE(saved_has_detail(*store.saved_get(plugin, slug))); // empty everywhere: not materialized
        REQUIRE(store.saved_detail_put({.plugin = plugin, .slug = slug, .synopsis_display = "only synopsis"}));
        CHECK(saved_has_detail(*store.saved_get(plugin, slug)));
        store.recompute_dup_groups(); // clustering runs over rows carrying detail
        CHECK(store.saved_get(plugin, slug)->dup_group == 0);
    }
}


TEST_CASE("Store: chapter flags bitfield (chapter-state 1.3-1.6)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md", slug = "cs-" + run;
    const auto seed = [&](const std::string& id, double n) {
        return ChapterSeed{id, n, "t" + id, ""};
    };

    SUBCASE("upsert preserves flags; identity key wins") {
        store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2)});
        store.chapter_flags_apply(plugin, slug, {"a1"}, false, 1, 0);
        store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("c3", 3)}); // re-parse
        CHECK(store.chapter_flags_get(plugin, slug, "a1") == 1); // bit survived
        CHECK(store.chapter_flags_get(plugin, slug, "b2") == 0);
        CHECK(store.chapter_flags_get(plugin, slug, "c3") == 0);
        CHECK_FALSE(store.chapter_flags_get(plugin, "nope", "a1").has_value());
    }

    SUBCASE("RMW set/clear/all via one call; map echoes new values") {
        store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2), seed("c3", 3)});
        auto m = store.chapter_flags_apply(plugin, slug, {"a1", "b2"}, false, 3, 0); // both bits
        CHECK(m["a1"] == 3);
        CHECK(m["b2"] == 3);
        m = store.chapter_flags_apply(plugin, slug, {"b2"}, false, 0, 1); // clear read
        CHECK(m["b2"] == 2);
        m = store.chapter_flags_apply(plugin, slug, {}, true, 0, 3); // all: clear both bits
        CHECK(m.size() == 3);
        for (auto& [id, v] : m) CHECK(v == 0);
    }

    SUBCASE("pages replace: delete+insert, count helper") {
        store.chapters_upsert(plugin, slug, {seed("a1", 1)});
        store.chapter_pages_replace(plugin, slug, "a1", {"p1", "p2", "p3"});
        CHECK(store.chapter_pages_count(plugin, slug, "a1") == 3);
        store.chapter_pages_replace(plugin, slug, "a1", {"x1"});
        CHECK(store.chapter_pages_count(plugin, slug, "a1") == 1); // old rows gone
        CHECK(store.chapter_pages_count(plugin, slug, "zz") == 0);
    }

    SUBCASE("list returns every row of the manga only") {
        store.chapters_upsert(plugin, slug, {seed("a1", 1)});
        store.chapters_upsert("other", slug, {seed("a1", 1)});
        store.chapter_flags_apply(plugin, slug, {"a1"}, false, 2, 0);
        const auto all = store.chapter_flags_list(plugin, slug);
        REQUIRE(all.size() == 1);
        CHECK(all.at("a1") == 2);
    }

    SUBCASE("failed transaction rolls back and leaves the connection usable") {
        // Regression: a throw between BEGIN and COMMIT used to leave the one
        // shared connection inside an open transaction, so every later BEGIN
        // failed with "cannot start a transaction within a transaction" and the
        // API stayed 500 for every title until the daemon restarted.
        store.chapters_upsert(plugin, slug, {seed("keep1", 1)}); // pre-existing row
        sqlite3* raw = nullptr;
        REQUIRE(sqlite3_open("test_store.db", &raw) == SQLITE_OK);
        const auto exec = [&](const char* sql) { return sqlite3_exec(raw, sql, nullptr, nullptr, nullptr); };
        REQUIRE(exec("DROP TRIGGER IF EXISTS t_store_wedge") == SQLITE_OK);
        REQUIRE(exec("CREATE TRIGGER t_store_wedge BEFORE INSERT ON chapters "
                     "BEGIN SELECT RAISE(ABORT, 'boom'); END") == SQLITE_OK);

        CHECK_THROWS(store.chapters_upsert(plugin, slug, {seed("keep1", 1), seed("boom2", 2)}));

        REQUIRE(exec("DROP TRIGGER t_store_wedge") == SQLITE_OK);
        sqlite3_close(raw);
        // The connection survived: a new transaction commits normally.
        CHECK(store.chapter_flags_apply(plugin, slug, {"keep1"}, false, 1, 0).at("keep1") == 1);
        store.chapters_upsert(plugin, slug, {seed("keep1", 1), seed("boom2", 2)});
        CHECK(store.chapter_flags_get(plugin, slug, "boom2") == 0);
    }
}

TEST_CASE("Store: read_max master progress + bump threshold (library-promote-on-save 4.4)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md", slug = "rm-" + run;
    store.chapters_upsert(plugin, slug, {ChapterSeed{"c1", 1, "t", "u", ""}, ChapterSeed{"c2", 2, "t", "u", ""}});
    store.chapter_pages_replace(plugin, slug, "c1", {"p1", "p2", "p3"});

    SUBCASE("bump merges max; auto-read at the stored page count") {
        CHECK(store.chapter_read_max_bump(plugin, slug, "c1", 2) == 2);
        CHECK((store.chapter_flags_get(plugin, slug, "c1").value_or(0) & 1) == 0); // 2/3: not done
        CHECK(store.chapter_read_max_bump(plugin, slug, "c1", 1) == 2); // max stays
        CHECK(store.chapter_read_max_bump(plugin, slug, "c1", 3) == 3);
        CHECK((store.chapter_flags_get(plugin, slug, "c1").value_or(0) & 1) != 0); // 3/3: read
    }

    SUBCASE("read_max survives a re-parse; absent rows read zero") {
        store.chapter_read_max_bump(plugin, slug, "c1", 2);
        store.chapters_upsert(plugin, slug, {ChapterSeed{"c1", 1, "t2", "u2", "x"}}); // identity refresh
        CHECK(store.chapter_read_max_list(plugin, slug)["c1"] == 2);
        CHECK(store.chapter_read_max_list(plugin, slug)["c2"] == 0);
        CHECK(store.chapter_read_max_bump(plugin, slug, "nope", 5) == 0);
    }

    SUBCASE("unsave wipes read_max with the pair") {
        store.chapter_read_max_bump(plugin, slug, "c1", 2);
        store.unsave_manga(plugin, slug);
        CHECK(store.chapter_read_max_list(plugin, slug).empty());
    }
}
TEST_CASE("Store: unsave purges every pair-keyed row (library-promote-on-save 4.1)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md", slug = "purge-" + run, other_slug = "keep-" + run;
    const std::string aid = "a" + run, bid = "b" + run; // per-run: shadow keys are global
    store.save_manga(SavedManga{.plugin = plugin, .slug = slug, .name = "Gone"});
    store.chapters_upsert(plugin, slug, {ChapterSeed{aid, 1, "t", "u", "2026-01-01"}});
    store.chapter_pages_replace(plugin, slug, aid, {"p1"});
    store.position_set(ReaderPosition{plugin, slug, aid, 1, 1, "", "", "", 0});
    store.upsert_update_attempt(plugin, slug, "ok", "");
    store.title_meta_put_chosen(plugin, slug, R"({"title":"T"})");
    store.state_set("host", std::string("host/novel/") + plugin + "/" + slug, "1");
    store.state_set("host", "keep-unrelated-" + run, "1");
    // neighbours that must survive: other slug, other plugin, non-pair cache rows
    store.chapters_upsert(plugin, other_slug, {ChapterSeed{bid, 1, "t", "u", ""}});
    store.api_cache_put(plugin, "h1", "{\"id\":\"" + slug + "\"}", "{}", 1, 1);
    store.api_cache_put(plugin, "h2", "{\"id\":\"" + slug + ":c1\"}", "{}", 1, 1); // page-scoped
    store.api_cache_put(plugin, "h3", "{\"id\":\"other\"}", "{}", 1, 1);
    store.api_cache_put("otherp", "h4", "{\"id\":\"" + slug + "\"}", "{}", 1, 1);

    store.unsave_manga(plugin, slug);

    CHECK_FALSE(store.saved_get(plugin, slug).has_value());
    CHECK(store.chapters_list(plugin, slug).empty());
    CHECK(store.chapter_pages_count(plugin, slug, aid) == 0);
    CHECK_FALSE(store.position_get(plugin, slug).has_value());
    CHECK_FALSE(store.update_attempt_get(plugin, slug).has_value());
    CHECK_FALSE(store.title_meta_get(plugin, slug).has_value());
    CHECK_FALSE(store.state_get("host", std::string("host/novel/") + plugin + "/" + slug).has_value());
    CHECK(store.state_get("host", "keep-unrelated-" + run).value_or("") == "1");
    CHECK_FALSE(store.api_cache_get(plugin, "h1").has_value());
    CHECK_FALSE(store.api_cache_get(plugin, "h2").has_value());
    CHECK(store.api_cache_get(plugin, "h3").has_value());
    CHECK(store.api_cache_get("otherp", "h4").has_value());
    CHECK(store.chapter_flags_get(plugin, other_slug, bid).has_value());

    // re-add starts fresh: row exists, no detail, no chapters
    store.save_manga(SavedManga{.plugin = plugin, .slug = slug, .name = "Again"});
    const auto row = store.saved_get(plugin, slug);
    REQUIRE(row.has_value());
    CHECK(row->name == "Again");
    CHECK(row->authors.empty());
    CHECK(store.chapters_list(plugin, slug).empty());
}

TEST_CASE("Store: chapters_list order + release_at round-trip/fingerprint (library-promote-on-save)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "md", slug = "rel-" + run;
    const auto seed = [](const std::string& id, double n, const std::string& rel) {
        return ChapterSeed{id, n, "t" + id, "", rel};
    };

    SUBCASE("reading order ascending; release_at verbatim") {
        store.chapters_upsert(plugin, slug, {seed("b2", 2, "2026-01-02"), seed("a1", 1, "1700000000")});
        const auto rows = store.chapters_list(plugin, slug);
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].chapter_id == "a1");
        CHECK(rows[0].number == 1);
        CHECK(rows[0].release_at == "1700000000");
        CHECK(rows[1].chapter_id == "b2");
        CHECK(rows[1].release_at == "2026-01-02");
    }

    SUBCASE("release-only re-parse lands (C3 fingerprint covers release_at)") {
        store.chapters_upsert(plugin, slug, {seed("a1", 1, "")});
        store.chapters_upsert(plugin, slug, {seed("a1", 1, "2026-05-05")});
        CHECK(store.chapters_list(plugin, slug)[0].release_at == "2026-05-05");
    }

    SUBCASE("unknown pair is empty") {
        CHECK(store.chapters_list(plugin, "nope-" + run).empty());
    }
}

TEST_CASE("Store: migrate_source rekeys the entry and carries the read boundary (library-migrate)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string old_p = "md", old_s = "mg-" + run;
    const std::string new_p = "fl", new_s = "mg2-" + run;
    const auto seed = [](const std::string& id, double n) { return ChapterSeed{id, n, "t" + id, ""}; };

    store.save_manga({old_p, old_s, "Old Name", "old.jpg", 0});
    store.chapters_upsert(old_p, old_s, {seed("a1", 1), seed("a2", 2), seed("a3", 3), seed("a4", 4), seed("a5", 5)});
    store.chapter_flags_apply(old_p, old_s, {"a3"}, false, 1, 0); // read through ch 3
    store.chapter_pages_replace(old_p, old_s, "a1", {"p1", "p2"});
    // target side: rows a future open would materialize + a preview title_meta
    store.chapters_upsert(new_p, new_s, {seed("b1", 1), seed("b2", 2), seed("b2b", 2.5), seed("b3", 3), seed("b9", 9)});
    store.chapter_flags_apply(new_p, new_s, {"b2"}, false, 1, 0); // previewed read on the target
    store.title_meta_put_fetched(old_p, old_s, R"([{"source":"x","fetched_at":5,"data":{}}])", 5);
    store.title_meta_put_chosen(new_p, new_s, R"({"title":"preview"})");

    SUBCASE("happy path: rekey, boundary carry, old rows gone, meta moved") {
        const bool ok = store.migrate_source(old_p, old_s, new_p, new_s,
                                             {.name = "New Name", .cover = "new.jpg", .status = "Ongoing", .total_ch = 5});
        CHECK(ok);

        const auto src = store.saved_get(old_p, old_s);
        CHECK_FALSE(src.has_value()); // old pair rekeyed away
        const auto dst = store.saved_get(new_p, new_s);
        REQUIRE(dst.has_value());
        CHECK(dst->name == "New Name");
        CHECK(dst->cover == "new.jpg");
        CHECK(dst->status == "Ongoing");
        CHECK(dst->total_ch == 5);
        CHECK(dst->read_ch == 4); // b1 b2 b2b b3 (<= 3); b9 stays unread
        CHECK(dst->added_at > 0); // library seniority rides the rekey
        CHECK((dst->status_flags & 2) == 0); // 4/5 read: NOT done

        // read boundary carried by number, sub-chapters included, previews kept
        CHECK(store.chapter_flags_get(new_p, new_s, "b1") == 1);
        CHECK(store.chapter_flags_get(new_p, new_s, "b2") == 1); // preview read, OR-preserving
        CHECK(store.chapter_flags_get(new_p, new_s, "b2b") == 1);
        CHECK(store.chapter_flags_get(new_p, new_s, "b3") == 1);
        CHECK(store.chapter_flags_get(new_p, new_s, "b9") == 0);

        // old-pair rows dropped
        CHECK(store.chapter_flags_list(old_p, old_s).empty());
        CHECK(store.chapter_pages_count(old_p, old_s, "a1") == 0);

        // title_meta: the entry's fetched history moved to the new pair
        const auto meta = store.title_meta_get(new_p, new_s);
        REQUIRE(meta.has_value());
        CHECK(meta->fetched.find("source") != std::string::npos);
    }

    SUBCASE("refusals: absent source, saved target, degenerate pair") {
        CHECK_FALSE(store.migrate_source("nope", old_s, new_p, new_s, {}));
        store.save_manga({new_p, new_s, "Taken", "", 0});
        CHECK_FALSE(store.migrate_source(old_p, old_s, new_p, new_s, {}));
        store.unsave_manga(new_p, new_s);
        CHECK_FALSE(store.migrate_source(old_p, old_s, old_p, old_s, {}));
        CHECK_FALSE(store.migrate_source("", old_s, new_p, new_s, {}));
    }
}

TEST_CASE("Store: maintenance snapshot + prune (backup knobs)") {
    TestStoreDb fixture;
    Store store(fixture.path);

    // isolated backup dir seeded with two stale snapshots: pruning keeps the
    // newest N across runs, older files go first (name == timestamp order).
    const std::string dir = "test_backup_dir";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    for (const char* name : {"library-20260101-000000.db", "library-20260102-000000.db"}) {
        std::ofstream(dir + "/" + name).put('x');
    }

    const auto snap1 = store.maintenance_vacuum_backup(dir, /*keep=*/2);
    REQUIRE(!snap1.empty());
    CHECK(std::filesystem::exists(snap1));
    CHECK(snap1.find("library-") != std::string::npos);

    // second run, keep=2: the two seeded old ones lose to the two newest
    const auto snap2 = store.maintenance_vacuum_backup(dir, 2);
    REQUIRE(!snap2.empty());
    int remaining = 0;
    for (const auto& it : std::filesystem::directory_iterator(dir)) {
        if (it.path().filename().string().rfind("library-", 0) == 0) {
            ++remaining;
        }
    }
    CHECK(remaining == 2);

    // snapshot is a valid sqlite db carrying the schema
    sqlite3* raw = nullptr;
    REQUIRE(sqlite3_open(snap2.c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM sqlite_master WHERE type='table'", -1, &st, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) > 0);
    sqlite3_finalize(st);
    sqlite3_close(raw);

    std::filesystem::remove_all(dir);
}

TEST_CASE("Store: dup-dirty flag coalescing (store-contention 1.4)") {
    Store store("test_store.db");
    CHECK(store.take_dup_dirty()); // ctor default true: first tick reclusters
    store.mark_dup_dirty();
    CHECK(store.take_dup_dirty());
    CHECK_FALSE(store.take_dup_dirty()); // exactly-once per mark
}

TEST_CASE("Store: raw-handle ops progress beside ORM ops (store-contention 2.2)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::atomic<int> cache_ok{0}, state_ok{0};
    std::string cache_err, state_err; // probe: which op throws, and its message
    std::mutex err_mu;
    std::thread cache_t([&] {
        for (int i = 0; i < 50; ++i) {
            const std::string k = "k" + std::to_string(i);
            try {
                store.api_cache_put("rawA_" + run, k, "{}", R"({"i":1})", i, 60);
                if (store.api_cache_get("rawA_" + run, k).has_value()) {
                    ++cache_ok;
                }
            } catch (const std::exception& e) {
                const std::lock_guard lk(err_mu);
                cache_err = std::string("i") + std::to_string(i) + ": " + e.what();
                return;
            }
        }
    });
    std::thread orm_t([&] {
        for (int i = 0; i < 50; ++i) {
            const std::string k = "s" + std::to_string(i);
            try {
                store.state_set("rawB_" + run, k, "v");
                if (store.state_get("rawB_" + run, k) == std::optional<std::string>{"v"}) {
                    ++state_ok;
                }
            } catch (const std::exception& e) {
                const std::lock_guard lk(err_mu);
                state_err = std::string("i") + std::to_string(i) + ": " + e.what();
                return;
            }
        }
    });
    cache_t.join();
    orm_t.join();
    INFO("cache_err=" << cache_err << " state_err=" << state_err);
    CHECK(cache_err.empty());
    CHECK(state_err.empty());
    CHECK(cache_ok == 50);
    CHECK(state_ok == 50);
}

TEST_CASE("Store: chapters_upsert fingerprint skip (store-contention 3.3)") {
    Store store("test_store.db");
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string plugin = "fp", slug = "s-" + run;
    const auto seed = [](const std::string& id, double n) {
        return ChapterSeed{id, n, "t" + id, "u" + id};
    };

    CHECK(store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2)}) == 2);
    CHECK(store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2)}) == 0); // identical: skipped

    // flag-only changes ride through the skip (flags are not fingerprinted)
    store.chapter_flags_apply(plugin, slug, {"a1"}, false, 1, 0);
    CHECK(store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2)}) == 0);
    CHECK(store.chapter_flags_get(plugin, slug, "a1") == 1);

    // any seed delta rewrites
    CHECK(store.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2), seed("c3", 3)}) == 3);

    // fingerprint persists in plugin_state: a reopened store skips too
    Store reopened("test_store.db");
    CHECK(reopened.chapters_upsert(plugin, slug, {seed("a1", 1), seed("b2", 2), seed("c3", 3)}) == 0);
}
