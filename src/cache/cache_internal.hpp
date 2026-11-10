#pragma once

/** \file cache_internal.hpp
 *  \brief Internal helpers shared by the cache TUs (monolith split for
 *  compile parallelism). Not a public surface; cache.hpp stays the API.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace cache {
namespace internal {

namespace fs = std::filesystem;

// Trailing dot-suffix of the key's path, sanitized; "bin" when absent.
inline std::string ext_of(const std::string& key) {
    const std::string path = key.substr(0, key.find('?'));
    const auto slash = path.find_last_of('/');
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 == name.size() || name.size() - dot > 6)
        return "bin";
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return "bin";
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

// v1 residue signature (D4): regular files sit directly in img/api/ and every
// one is sha-named; any subdir or foreign name is live v2 layout -- keep it.
inline bool legacy_v1(const fs::path& root) {
    bool seen = false;
    for (const char* kind : {"img"}) {
        std::error_code ec;
        fs::directory_iterator it(root / kind, ec), end{};
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code fec;
            if (it->is_directory(fec)) return false;
            if (!it->is_regular_file(fec) || fec) continue;
            const std::string name = it->path().filename().string();
            if (!name.empty() && name[0] == '.') continue; // .tmp leftovers
            const std::string stem = it->path().stem().string();
            if (stem.size() != 40 || // 40 = sha1 hex
                !std::all_of(stem.begin(), stem.end(),
                             [](unsigned char c) { return std::isxdigit(c); }))
                return false;
            seen = true;
        }
    }
    return seen;
}

// V3 layout helpers: cover / chapter dir / hash fallback rel_path builders.
// Cover ratio rides in the filename as fixed-point 4 digits (no dot):
// <hash>-6988.webp == ratio 0.6988. Sidecar .json files are gone.
inline std::string cover_rel(const std::string& plugin, const std::string& url_hash,
                             const std::string& ext, double ratio) {
    const int r4 = ratio > 0.0 ? static_cast<int>(std::lround(ratio * 10000.0)) : 0;
    return plugin + "/cover/" + url_hash + "-" + std::to_string(r4) + "." + ext;
}

/** \brief Ratio from a cover filename tail "<hash>-<r4>.<ext>"; 0.0 when
 *  absent/zero (old files without the suffix keep serving, no header). */
inline double ratio_from_name(const std::string& name) {
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return 0.0;
    const auto dash = name.find_last_of('-', dot);
    if (dash == std::string::npos) return 0.0;
    const std::string digits = name.substr(dash + 1, dot - dash - 1);
    if (digits.empty() || digits.size() > 5 ||
        !std::all_of(digits.begin(), digits.end(), ::isdigit))
        return 0.0;
    return static_cast<double>(std::stoi(digits)) / 10000.0;
}

inline std::string page_rel(const std::string& plugin, const std::string& slug, int chapter, int page) {
    // pad(n) = min-3 zero-padding: ch 2 -> ch002, ch 12 -> ch012, ch 123 -> ch123
    const auto pad = [](int n) {
        auto s = std::to_string(n);
        while (s.size() < 3) s = "0" + s;
        return s;
    };
    // ponytail: number-only dir suffix (lo's call). Fractional numbers keep
    // '.' (fs-safe); one chapter dir shared by all its pages. If duplicate
    // chapter numbers across languages ever collide, re-add an id8 suffix
    // derived from the chapter ID.
    return plugin + "/manga/" + slug + "/ch" + pad(chapter) + "/p" + pad(page);
}

} // namespace internal
} // namespace cache
