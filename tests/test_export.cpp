#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "export_cbz.hpp"

#include "cache/slug.hpp"

namespace fs = std::filesystem;

namespace {

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_cbz_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void put(const fs::path& p, const std::string& body) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << body;
}

bool zip_has(const std::string& zip, const std::string& name) {
    return zip.find(name) != std::string::npos;
}

// Local-file-header order: the FIRST occurrence of the entry name is the LFH
// name field, which precedes the central directory. Returns npos when absent.
std::size_t lfh_offset(const std::string& zip, const std::string& name) {
    return zip.find(name);
}

} // namespace

TEST_CASE("empty slug is an empty valid zip") {
    TempRoot tmp;
    const auto zip = cbz::build(tmp.path.string(), "mangadex", "");
    REQUIRE(zip);
    CHECK(zip->size() >= 22);
    CHECK(zip->substr(zip->size() - 22, 4) == "PK\x05\x06");
}

TEST_CASE("v3 pages land under reading-order names; ch filter narrows") {
    TempRoot tmp;
    const auto mx = cache::slug_dir("slug-X");
    put(tmp.path / "img/mangadex/manga" / mx / "ch003/p002.webp", "P2");
    put(tmp.path / "img/mangadex/manga" / mx / "ch003/p001.webp", "P1");
    put(tmp.path / "img/mangadex/manga" / mx / "ch004/p001.webp", "P4");
    put(tmp.path / "img/mangadex/manga" / mx / "ch003-a1b2c3d4/p001.webp", "DUP");
    put(tmp.path / "img/mangadex/manga/other/ch001/p001.webp", "NOPE");

    const auto all = cbz::build(tmp.path.string(), "mangadex", "slug-X");
    REQUIRE(all);
    CHECK(zip_has(*all, "ch003/p001.webp"));
    CHECK(zip_has(*all, "ch003/p002.webp"));
    CHECK(zip_has(*all, "ch004/p001.webp"));
    CHECK(zip_has(*all, "ch003-a1b2c3d4/p001.webp"));
    CHECK_FALSE(zip_has(*all, "other"));

    const auto one = cbz::build(tmp.path.string(), "mangadex", "slug-X/ch003");
    REQUIRE(one);
    CHECK(zip_has(*one, "ch003/p001.webp"));
    CHECK(zip_has(*one, "ch003-a1b2c3d4/p001.webp"));
    CHECK_FALSE(zip_has(*one, "ch004/p001.webp"));
}

// Regression: archive order = reading order. ch999 must precede ch1000 even
// though "ch1000" sorts before "ch999" lexicographically (different padding
// width). Asserted on the local file header offsets, which is the true entry
// order inside the zip.
TEST_CASE("chapter order crossing the padding width stays numeric (ch999 before ch1000)") {
    TempRoot tmp;
    const auto sy = cache::slug_dir("slug-Y");
    put(tmp.path / "img/mangadex/manga" / sy / "ch999/p001.webp", "C999P1");
    put(tmp.path / "img/mangadex/manga" / sy / "ch1000/p001.webp", "C1000P1");

    const auto zip = cbz::build(tmp.path.string(), "mangadex", "slug-Y");
    REQUIRE(zip);
    CHECK(zip_has(*zip, "ch999/p001.webp"));
    CHECK(zip_has(*zip, "ch1000/p001.webp"));

    const std::size_t off999 = lfh_offset(*zip, "ch999/p001.webp");
    const std::size_t off1000 = lfh_offset(*zip, "ch1000/p001.webp");
    REQUIRE(off999 != std::string::npos);
    REQUIRE(off1000 != std::string::npos);
    CHECK(off999 < off1000); // ch999 first: reading order, not lexicographic
}
