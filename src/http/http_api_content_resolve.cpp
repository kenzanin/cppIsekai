#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace http::api {

// Error string from a plugin envelope, for propagating a real upstream
// failure instead of the generic "unknown chapter" 404.
std::string envelope_error(const std::string& env) {
    glz::generic g;
    if (glz::read_json(g, env) || !g.is_object()) {
        return {};
    }
    const auto& obj = g.get_object();
    const auto it = obj.find("error");
    if (it == obj.end()) {
        return {};
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data)) {
        return *s;
    }
    return {};
}

// Resolve the chapter id for the pages call: the `ch_id` query param picks the
// exact entry when it matches a list entry, else the first number match. An
// absent or unmatched ch_id behaves exactly as the number-only path (design
// D1/D2). Used twice (before the pages call and inside build_pages_response)
// so duplicates can never resolve differently on the two paths.
std::string resolve_chapter_id(const std::string& chs_env, const std::string& wanted, double chd) {
    const auto chs = envelope_data(chs_env);
    if (!chs) {
        return {};
    }
    const auto entry_id = [](const auto& item) -> std::string {
        if (!item.is_object()) {
            return {};
        }
        const auto& entry = item.get_object();
        const auto* const idf = entry.find("id");
        if (idf == entry.end()) {
            return {};
        }
        if (const auto* s = std::get_if<std::string>(&idf->second.data)) {
            return *s;
        }
        return {};
    };
    if (!wanted.empty()) {
        for (const auto& item : *chs) {
            if (entry_id(item) == wanted) {
                return wanted;
            }
        }
    }
    // Number match with ties going to the id-min duplicate: pages_api sorts
    // equal numbers by id, so the neighbor chain starts at the id-min entry --
    // the fallback must land on that same release, not the raw-first one
    // (mangafreak emits `_24b` before `_24a`).
    std::string best;
    for (const auto& item : *chs) {
        if (!item.is_object()) {
            continue;
        }
        const auto& entry = item.get_object();
        const auto* const num = entry.find("number");
        if (num == entry.end()) {
            continue;
        }
        const auto* d = std::get_if<double>(&num->second.data);
        if (!d || *d != chd) {
            continue;
        }
        const std::string id = entry_id(item);
        if (!id.empty() && (best.empty() || id < best)) {
            best = id;
        }
    }
    return best;
}

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
