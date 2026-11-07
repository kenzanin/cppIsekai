#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <map>
#include <string>

#include "cache/slug.hpp"

namespace {

// fs-safe: lowercase ascii, '-', or a UTF-8 byte (>= 0x80).
bool fs_safe(const std::string& s) {
    for (char c : s) {
        const auto u = static_cast<unsigned char>(c);
        const bool ok = (u >= 0x80) || c == '-' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!ok)
            return false;
    }
    return true;
}

} // namespace

TEST_CASE("empty and punctuation-only titles fall back") {
    CHECK(cache::slug_base("") == "untitled");
    CHECK(cache::slug_base("   !!! --- ?? ") == "untitled");
}

TEST_CASE("slug is lowercase, fs-safe, separators collapsed") {
    CHECK(cache::slug_base("One Piece!!") == "one-piece");
    CHECK(cache::slug_base("  --Hi!!! There?? ") == "hi-there");
    const std::string s = cache::slug_base("Frieren: 葬送のフリーレン!");
    CHECK(s == "frieren-葬送のフリーレン"); // unicode kept, punct trimmed
    CHECK(fs_safe(s));
    CHECK(fs_safe(cache::slug_base("葬送のフリーレン")));
}

TEST_CASE("length cap 64, unicode boundary-safe") {
    CHECK(cache::slug_base(std::string(100, 'a')).size() == 64);
    // 63 ascii + 2-byte 'ß' -> the cap backs off to the previous boundary.
    const auto mixed = cache::slug_base(std::string(63, 'a') + "ßx");
    CHECK(mixed.size() == 63);
    CHECK((static_cast<unsigned char>(mixed.back()) & 0xC0) != 0x80); // ends on lead byte
}

TEST_CASE("suffix only when base slugs collide; never for a lone manga") {
    const std::map<std::string, std::string> one{{"aaaa1111", "Solo Leveling"}};
    CHECK(cache::manga_slug(one, "aaaa1111") == "solo-leveling");

    const std::map<std::string, std::string> many{
        {"11111111", "Solo Leveling"},
        {"22222222", "Solo Leveling"},
        {"x1234567890", "Solo Leveling"},
        {"33333333", "Naruto"},
    };
    CHECK(cache::manga_slug(many, "33333333") == "naruto"); // no collision
    CHECK(cache::manga_slug(many, "11111111") == "solo-leveling-11111111");
    CHECK(cache::manga_slug(many, "22222222") == "solo-leveling-22222222");
    CHECK(cache::manga_slug(many, "x1234567890") == "solo-leveling-34567890"); // last 8 only
    CHECK(fs_safe(cache::manga_slug(many, "11111111")));
    CHECK(cache::manga_slug(many, "missing").empty());
}

TEST_CASE("collision slug stays within the 64-char cap") {
    const std::map<std::string, std::string> two{{"11111111", std::string(100, 'a')},
                                                 {"22222222", std::string(100, 'a')}};
    const auto s = cache::manga_slug(two, "22222222");
    CHECK(s == std::string(55, 'a') + "-22222222");
    CHECK(s.size() == cache::slug_max);
}
