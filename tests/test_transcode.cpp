// Transcode core (cache-layout-v3 1.2): vips buffer in/out, cover resize,
// passthrough on junk. Hermetic: tiny generated buffers, no disk, no network.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cache/transcode.hpp"

#include <vips/vips.h>

#include <string>

namespace {

std::string tiny_webp() {
    VipsImage* img = nullptr;
    REQUIRE(vips_black(&img, 8, 6, nullptr) == 0);
    void* buf = nullptr;
    size_t len = 0;
    REQUIRE(vips_webpsave_buffer(img, &buf, &len, nullptr) == 0);
    std::string out(static_cast<char*>(buf), len);
    g_free(buf);
    g_object_unref(img);
    return out;
}

} // namespace

TEST_CASE("transcode: webp in -> webp out keeps dims") {
    const std::string in = tiny_webp();
    const cache::TranscodeResult r = cache::transcode(in, "webp", 0);
    CHECK(r.ext == "webp");
    CHECK(r.width == 8);
    CHECK(r.height == 6);
    CHECK(r.bytes.rfind("RIFF", 0) == 0); // webp container
}

TEST_CASE("transcode: cover resize fits longest side, keeps aspect") {
    const std::string in = tiny_webp(); // 8x6
    const cache::TranscodeResult r = cache::transcode(in, "webp", 4);
    CHECK(r.ext == "webp");
    CHECK(r.width == 4);
    CHECK(r.height == 3); // 8:6 -> 4:3
}

TEST_CASE("transcode: junk input passes through unchanged") {
    const std::string junk = "not an image at all";
    const cache::TranscodeResult r = cache::transcode(junk, "webp", 800);
    CHECK(r.ext.empty());
    CHECK(r.bytes == junk);
    CHECK(r.width == 0);
}

TEST_CASE("transcode: none is passthrough") {
    const std::string in = tiny_webp();
    const cache::TranscodeResult r = cache::transcode(in, "none", 0);
    CHECK(r.ext.empty());
    CHECK(r.bytes == in);
}
