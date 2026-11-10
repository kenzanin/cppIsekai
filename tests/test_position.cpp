#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <optional>

#include "http/position_api.hpp"

TEST_CASE("position: absent mode/direction keep the stored values") {
    const std::optional<ReaderPosition> stored = ReaderPosition{"md", "m", "c1", 12.5, 3, "webtoon", "", 0};

    auto row = merge_position(stored, {"md", "m", "c2", 0, 4, "", "", 0});
    CHECK(row.mode == "webtoon"); // absent -> stored survives
    CHECK(row.direction.empty()); // absent and nothing stored -> empty
    CHECK(row.chapter_id == "c2"); // identity/page always from the request
    CHECK(row.page == 4);

    row = merge_position(stored, {"md", "m", "c2", 0, 4, "spread-h", "ltr", 0});
    CHECK(row.mode == "spread-h"); // explicit beats stored
    CHECK(row.direction == "ltr");
}

TEST_CASE("position: no stored row -> fields exactly as requested") {
    const std::optional<ReaderPosition> stored;
    const auto row = merge_position(stored, {"md", "m", "c1", 0, 1, "", "rtl", 0});
    CHECK(row.mode.empty());
    CHECK(row.direction == "rtl");
    CHECK(row.chapter_id == "c1");
}
