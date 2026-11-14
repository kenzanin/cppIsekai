#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <BS_thread_pool.hpp>

#include <chrono>
#include <string>

#include "http/api_ctx.hpp"
#include "store/store.hpp"

namespace {

std::string chapters_env(int count) {
    std::string env = R"({"ok":true,"data":[)";
    for (int i = 1; i <= count; ++i) {
        if (i > 1) {
            env += ',';
        }
        env += R"({"id":"c)" + std::to_string(i) + R"(","number":)" + std::to_string(i) + "}";
    }
    env += "]}";
    return env;
}

} // namespace

// auto-update G1: the chapters-only refresh path (the scheduler never calls
// detail) must still advance total_ch and raise the New bit when the chapter
// list grew, and must not invent a New badge on a flat refresh.
TEST_CASE("saved stats: chapters-only sync advances total_ch and the New bit") {
    const std::string path = "test_saved_stats.db";
    std::remove(path.c_str());
    Store store(path);
    BS::thread_pool<> db(1);
    const std::string slug = "sched-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    store.save_manga({"md", slug, "Sched", "", 0});

    sync_saved_stats(store, db, "md", slug, "", chapters_env(3)); // no detail envelope
    auto row = store.saved_get("md", slug);
    REQUIRE(row.has_value());
    CHECK(row->total_ch == 3);           // the chapter count landed
    CHECK((row->status_flags & 1) != 0); // growth -> New

    store.saved_clear_update("md", slug); // reader saw it: badge down
    sync_saved_stats(store, db, "md", slug, "", chapters_env(3));
    row = store.saved_get("md", slug);
    REQUIRE(row.has_value());
    CHECK(row->total_ch == 3);
    CHECK((row->status_flags & 1) == 0); // flat refresh: no spurious New

    sync_saved_stats(store, db, "md", slug, "", chapters_env(4)); // one more chapter
    row = store.saved_get("md", slug);
    REQUIRE(row.has_value());
    CHECK(row->total_ch == 4);
    CHECK((row->status_flags & 1) != 0);
}

TEST_CASE("saved stats: chapters-only sync on an unsaved row is a no-op") {
    const std::string path = "test_saved_stats.db";
    Store store(path);
    BS::thread_pool<> db(1);
    const std::string slug = "absent-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    sync_saved_stats(store, db, "md", slug, "", chapters_env(2));
    CHECK_FALSE(store.saved_get("md", slug).has_value());
}

// The detail envelope drives the status + its category bit (bit2-5); the
// chapters-only path leaves the stored status alone.
TEST_CASE("saved stats: detail envelope sets the status, chapters-only keeps it") {
    const std::string path = "test_saved_stats.db";
    Store store(path);
    BS::thread_pool<> db(1);
    const std::string slug = "detail-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    store.save_manga({"md", slug, "Detail", "", 0});

    const std::string detail = R"({"ok":true,"data":{"name":"Detail","status":"hiatus"}})";
    sync_saved_stats(store, db, "md", slug, detail, chapters_env(2));
    auto row = store.saved_get("md", slug);
    REQUIRE(row.has_value());
    CHECK(row->status == "hiatus");
    CHECK((row->status_flags & 8) != 0); // category bit3 = hiatus

    sync_saved_stats(store, db, "md", slug, "", chapters_env(2)); // chapters-only: status stays
    row = store.saved_get("md", slug);
    REQUIRE(row.has_value());
    CHECK(row->status == "hiatus");
    CHECK((row->status_flags & 8) != 0);
}

// inject_status_flags rides the stored bitfield into the detail envelope's
// data object; a non-object envelope is left alone.
TEST_CASE("saved stats: inject_status_flags writes the bitfield into the envelope") {
    const std::string env = R"({"ok":true,"data":{"name":"X"}})";
    const std::string out = detail::inject_status_flags(env, 66);
    CHECK(out.find(R"("status_flags":66)") != std::string::npos);
    CHECK(detail::inject_status_flags("not json", 1) == "not json");
    CHECK(detail::inject_status_flags(R"({"ok":true,"data":[1]})", 1) == R"({"ok":true,"data":[1]})");
}
