#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "http/img.hpp"

namespace fs = std::filesystem;

namespace {

// Canned bodies per key; anything absent fails the fetch (hermetic offline).
struct FakeState {
    std::map<std::string, std::string> bodies;
    int calls{};

    std::optional<std::string> get(const std::string& key) {
        ++calls;
        const auto it = bodies.find(key);
        if (it == bodies.end()) return std::nullopt;
        return it->second;
    }
};

struct TempRoot {
    fs::path path;
    TempRoot()
        : path(fs::temp_directory_path() /
               ("cppisekai_img_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

cache::Cache fake_cache(const std::string& root, const std::shared_ptr<FakeState>& state) {
    return cache::Cache(cache::Config{root, 1 << 20},
                        [state](const std::string& k) { return state->get(k); });
}

} // namespace

TEST_CASE("img: content-type by extension, octet-stream fallback") {
    CHECK(image_mime("a.jpg") == "image/jpeg");
    CHECK(image_mime("a.jpeg") == "image/jpeg");
    CHECK(image_mime("dir/x.png") == "image/png");
    CHECK(image_mime("a.gif") == "image/gif");
    CHECK(image_mime("a.webp") == "image/webp");
    CHECK(image_mime("a.avif") == "image/avif");
    CHECK(image_mime("a.svg") == "image/svg+xml");
    CHECK(image_mime("a.bin") == "application/octet-stream");
    CHECK(image_mime("noext") == "application/octet-stream");
}

TEST_CASE("img: miss fetches+stores+serves; second view comes from disk") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    state->bodies = {{"http://x/a.jpg", "JPEGBYTES"}};
    cache::Cache cache = fake_cache(root.path.string(), state);

    const ImgReply first = serve_cached_image(cache, "http://x/a.jpg");
    CHECK(first.code == 200);
    CHECK(first.body == "JPEGBYTES");
    CHECK(first.mime == "image/jpeg");
    REQUIRE(state->calls == 1);

    // fetcher now fails: the second view must be served from disk regardless
    state->bodies.clear();
    const ImgReply again = serve_cached_image(cache, "http://x/a.jpg");
    CHECK(again.code == 200);
    CHECK(again.body == "JPEGBYTES");
    CHECK(again.mime == "image/jpeg");
    CHECK(state->calls == 1); // no refetch: disk, not network
}

TEST_CASE("img: offline miss is 404 with empty body") {
    TempRoot root;
    auto state = std::make_shared<FakeState>();
    cache::Cache cache = fake_cache(root.path.string(), state);

    const ImgReply miss = serve_cached_image(cache, "http://x/none.webp");
    CHECK(miss.code == 404);
    CHECK(miss.body.empty());
    CHECK(state->calls == 1);
}
