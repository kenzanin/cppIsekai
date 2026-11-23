#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "assets/assets.hpp"

#include <crow/mustache.h>
#include <libzippp/libzippp.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("cppisekai-assets-test-" + std::to_string(::getpid()));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("assets: zip root loads into memory and serves templates + statics") {
    TempDir tmp;
    const fs::path zip = tmp.path / "assets.zip";
    {
        libzippp::ZipArchive z(zip.string());
        REQUIRE(z.open(libzippp::ZipArchive::New));
        const std::string page = "hi {{name}} {{>part.mustache}}", part = "[part]";
        const std::string css("\x1b\x8b", 2), png = "PNG";
        CHECK(z.addData("templates/page.mustache", page.data(), page.size()));
        CHECK(z.addData("templates/part.mustache", part.data(), part.size()));
        CHECK(z.addData("static/app.css.br", css.data(), css.size()));
        CHECK(z.addData("icon/tray.png", png.data(), png.size()));
        z.close();
    }

    assets::init(zip.string());
    CHECK(assets::in_memory());
    const auto* page = assets::read("templates/page.mustache");
    REQUIRE(page != nullptr);
    CHECK_EQ(*page, "hi {{name}} {{>part.mustache}}");
    CHECK(assets::read("static/app.css.br") != nullptr);
    CHECK(assets::read("icon/tray.png") != nullptr);
    CHECK(assets::read("static/missing.js") == nullptr);

    // Crow's partial resolution must go through the same in-memory loader.
    const auto rendered = crow::mustache::load("page.mustache").render(crow::mustache::context{{"name", "bro"}});
    CHECK_EQ(rendered.body_, "hi bro [part]");

    const auto icon = assets::icon_path("assets/tray.png");
    CHECK(icon.is_absolute());
    CHECK(fs::exists(icon));
    {
        std::ifstream f(icon, std::ios::binary);
        CHECK_EQ(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()), "PNG");
    }
    assets::shutdown();
    CHECK_FALSE(assets::in_memory());
    CHECK_FALSE(fs::exists(icon));
}

TEST_CASE("assets: folder root loads and an empty path keeps disk mode") {
    TempDir tmp;
    fs::create_directories(tmp.path / "static");
    {
        std::ofstream(tmp.path / "static" / "x.js", std::ios::binary) << "ok";
    }
    assets::init(tmp.path.string());
    const auto* js = assets::read("static/x.js");
    REQUIRE(js != nullptr);
    CHECK_EQ(*js, "ok");
    assets::shutdown();

    assets::init("");
    CHECK_FALSE(assets::in_memory());
    CHECK(assets::read("static/x.js") == nullptr);
}
