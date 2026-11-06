#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

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
}
