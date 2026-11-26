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

TEST_CASE("cache_ns: the display name keys the cache, the id is only the fallback") {
    // The point of the whole exercise: the on-disk directory follows the plugin
    // display NAME, so swapping the plugin id or its folder keeps the cached
    // files. These two lines are the requirement.
    CHECK(cache::cache_ns("mangadex", "MangaDex") == "mangadex");
    CHECK(cache::cache_ns("some-renamed-folder", "MangaDex") == cache::cache_ns("mangadex", "MangaDex"));

    // Blank or unslugifiable name -> the id still works as a namespace.
    CHECK(cache::cache_ns("mangadex", "") == "mangadex");
    CHECK(cache::cache_ns("mangadex", "   \t\n") == "mangadex");
    CHECK(cache::cache_ns("mangadex", "!!!") == "mangadex");

    // Still fs-safe, lowercase and capped like every other slug.
    CHECK(cache::cache_ns("x", "Atsu.moe Reader") == "atsu-moe-reader");
    CHECK(fs_safe(cache::cache_ns("x", "Atsu.moe Reader")));
    CHECK(cache::cache_ns("x", std::string(100, 'A')).size() == cache::slug_max);
}

TEST_CASE("slug_dir: uniform bound, stable, collision-separated") {
    // Every slug, short or 202-char, gets the same shape: prefix'-hash8.
    const auto simple = cache::slug_dir("one-piece");
    CHECK(simple.starts_with("one-piece-"));
    CHECK(fs_safe(simple));
    CHECK(simple.size() == 18); // "one-piece" (9) + '-' + 8 hash hex
    CHECK(cache::slug_dir("one-piece") == simple);
    CHECK(simple.size() <= 49);

    // The real 4ucomics slug (202 chars) is bounded; its 40-prefix survives.
    const auto long_a = cache::slug_dir(std::string(202, 'a'));
    CHECK(long_a.size() == 49);
    CHECK(long_a.substr(0, 41) == std::string(40, 'a') + '-');

    // A different title sharing the first 40 chars still lands elsewhere.
    const auto long_b = cache::slug_dir(std::string(100, 'a') + "b" + std::string(101, 'a'));
    CHECK(long_a != long_b);

    // UTF-8: the prefix never splits a codepoint (slug_dir takes an
    // already-slugged id; no case folding here).
    const auto jp = cache::slug_dir("frieren-葬送のフリーレン-pad-pad-pad-pad-pad-pad");
    CHECK(jp.size() <= 49);
    CHECK(jp.starts_with("frieren-"));
    CHECK(fs_safe(jp));
}
