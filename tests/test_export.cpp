#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "export_cbz.hpp"

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
    put(tmp.path / "img/mangadex/manga/slug-X/ch003/p002.webp", "P2");
    put(tmp.path / "img/mangadex/manga/slug-X/ch003/p001.webp", "P1");
    put(tmp.path / "img/mangadex/manga/slug-X/ch004/p001.webp", "P4");
    put(tmp.path / "img/mangadex/manga/slug-X/ch003-a1b2c3d4/p001.webp", "DUP");
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
