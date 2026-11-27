#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace http::api {

// cache-full check for one chapter: <root>/img/<ns>/manga/<slug>/<chdir>/pNNN.ext
// count >= pages. ch-dir matched by chNNN prefix (collision id8 variant counts too).
// `ns` is the cache namespace from Cache::ns(), not the raw plugin id.
bool chapter_cached(const std::string& ns, const std::string& slug, int chapter, int pages) {
    if (chapter < 0 || pages <= 0) { // ch 0 is a real chapter (comico prologue)
        return false;
    }
    const fs::path base = fs::path("data/cache") / "img" / ns / "manga" / cache::slug_dir(slug);
    const std::string prefix = "ch" +
                               (chapter < 10    ? std::string("00")
                                : chapter < 100 ? std::string("0")
                                                : std::string()) +
                               std::to_string(chapter);
    std::error_code ec;
    if (!fs::is_directory(base, ec)) {
        return false;
    }
    for (const auto& ch : fs::directory_iterator(base, ec)) {
        if (ec) {
            return false;
        }
        if (!ch.is_directory(ec)) {
            continue;
        }
        if (!ch.path().filename().string().starts_with(prefix)) {
            continue;
        }
        int n = 0;
        for (const auto& p : fs::directory_iterator(ch.path(), ec)) {
            if (!ec && p.is_regular_file(ec)) {
                ++n;
            }
        }
        return n >= pages;
    }
    return false;
}

} // namespace http::api
