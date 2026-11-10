#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>

#include "store/store.hpp"

TEST_CASE("Store: seed, plugins, search, count") {
    Store store("test_store.db"); // fresh file -> seed path runs

    SUBCASE("plugins lists 4 with 12 titles each") {
        auto plugins = store.plugins();
        REQUIRE(plugins.size() == 4);
        for (const auto& p : plugins) {
            CHECK(p.titles == 12);
        }
        CHECK(plugins[0].name == "Asura"); // order_by name
    }

    SUBCASE("count without filter") {
        CHECK(store.count("", "") == 48);
    }

    SUBCASE("count with plugin filter") {
        CHECK(store.count("Asura", "") == 12);
        CHECK(store.count("Nope", "") == 0);
    }

    SUBCASE("count with LIKE escaping: metacharacters stay literal") {
        CHECK(store.count("", "_") == 0);        // no literal underscore in seeds
        CHECK(store.count("", "%") == 0);        // % must not act as wildcard
        CHECK(store.count("", "Scans #3") == 4); // literal # matches
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
        store.position_set({"md", slug, "ch-uuid-1", 12.5, 3, "spread-h", "rtl", 0});
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
        store.position_set({"md", slug, "ch-uuid-2", 13, 1, "", "", 0});
        const auto upd = store.position_get("md", slug);
        REQUIRE(upd.has_value());
        CHECK(upd->chapter_id == "ch-uuid-2");
        CHECK(upd->page == 1);
        CHECK(upd->mode.empty());
        CHECK(upd->direction.empty());

        store.position_set({"md", slug + "x", "c", 1, 1, "", "", 0}); // neighbor manga isolated
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
