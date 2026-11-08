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

TEST_CASE("cover and pages land under reading-order names") {
    TempRoot tmp;
    put(tmp.path / "img/mangadex/covers/slug-X.jpg", "COVER");
    put(tmp.path / "img/mangadex/slug-X/ch003/p002.webp", "PAGE");
    put(tmp.path / "img/mangadex/other/ch001/p001.webp", "NOPE");
    const auto zip = cbz::build(tmp.path.string(), "mangadex", "slug-X");
    REQUIRE(zip);
    CHECK(zip_has(*zip, "cover.jpg"));
    CHECK(zip_has(*zip, "ch003/p002.webp"));
    CHECK_FALSE(zip_has(*zip, "NOPE"));
}
