#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <optional>

#include "http/position_api.hpp"

TEST_CASE("position: absent mode/direction keep the stored values") {
    const std::optional<ReaderPosition> stored = ReaderPosition{"md", "m", "c1", 12.5, 3, "webtoon", "", "fit-w", 0};

    auto row = merge_position(stored, {"md", "m", "c2", 0, 4, "", "", "", 0});
    CHECK(row.mode == "webtoon"); // absent -> stored survives
    CHECK(row.direction.empty()); // absent and nothing stored -> empty
    CHECK(row.zoom == "fit-w"); // absent zoom keeps stored
    CHECK(row.chapter_id == "c2"); // identity/page always from the request
    CHECK(row.page == 4);

    row = merge_position(stored, {"md", "m", "c2", 0, 4, "spread-h", "ltr", "1:1", 0});
    CHECK(row.mode == "spread-h"); // explicit beats stored
    CHECK(row.direction == "ltr");
}

TEST_CASE("position: no stored row -> fields exactly as requested") {
    const std::optional<ReaderPosition> stored;
    const auto row = merge_position(stored, {"md", "m", "c1", 0, 1, "", "rtl", "", 0});
    CHECK(row.mode.empty());
    CHECK(row.direction == "rtl");
    CHECK(row.chapter_id == "c1");
}

TEST_CASE("position: resolve_chapter_number — ch 0 is valid (falsy-zero)") {
    const std::optional<ReaderPosition> stored = ReaderPosition{"md", "m", "c1", 3, 1, "", "", "", 0};

    CHECK(resolve_chapter_number(stored, std::nullopt) == 3.0); // absent keeps stored
    CHECK(resolve_chapter_number(std::nullopt, std::nullopt) == 0.0); // nothing stored, nothing sent
    CHECK(resolve_chapter_number(stored, 0.0) == 0.0); // explicit 0 wins over stored 3
    CHECK(resolve_chapter_number(stored, 1.5) == 1.5); // fractional passes through
    CHECK(resolve_chapter_number(std::nullopt, 0.0) == 0.0); // explicit 0, no stored row

    // full request-path shape: stored ch 0, absent chapter_number -> stays 0, not 1
    const std::optional<ReaderPosition> stored_zero = ReaderPosition{"md", "m", "c0", 0, 1, "", "", "", 0};
    CHECK(resolve_chapter_number(stored_zero, std::nullopt) == 0.0);
    CHECK(resolve_chapter_number(stored_zero, 1.0) == 1.0); // jump away from 0
}
