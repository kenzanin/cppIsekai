#include "export_cbz.hpp"

#include "cache/slug.hpp"

#include <algorithm>
#include <libzippp/libzippp.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace cbz {
namespace {

// Empty-but-valid ZIP = 22-byte EOCD only (no central directory, no entries).
// Matches spec scenario: "empty slug -> empty-but-valid zip".
constexpr char kEmptyZip[] = "\x50\x4b\x05\x06" // End-of-central-dir sig
                             "\x00\x00\x00\x00" // disk #
                             "\x00\x00\x00\x00" // entries on disk
                             "\x00\x00\x00\x00" // total entries
                             "\x00\x00\x00\x00" // central dir size
                             "\x00\x00\x00\x00" // central dir offset
                             "\x00\x00";        // comment length

std::optional<std::string> zip_of(const fs::path& tmp, const std::vector<std::pair<std::string, fs::path>>& entries) {
    std::optional<std::string> out;
    {
        libzippp::ZipArchive zip(tmp.string());
        if (!zip.open(libzippp::ZipArchive::New)) {
            return std::nullopt;
        }
        for (const auto& [name, path] : entries) {
            if (!zip.addFile(name, path.string())) {
                zip.discard();
                return std::nullopt;
            }
        }
        // IMPORTANT: for an empty archive libzippp's close() calls
        // zip_discard() internally (nothing to write), which deletes the
        // temp file. We detect this and fall through to the empty-ZIP path
        // below rather than returning nullopt.
        const bool ok_close = zip.close() == LIBZIPPP_OK;
        (void)ok_close; // diagnostic: unused
    }
    // Check whether the temp file survived the close.
    std::error_code ec;
    const auto sz = fs::file_size(tmp, ec);
    if (ec || sz == 0) {
        // Empty archive -> emit raw EOCD (spec scenario: "empty zip")
        constexpr char kEocd[22] = {
            0x50, 0x4b, 0x05, 0x06, // sig
            0,    0,                // disk num
            0,    0,                // disk w/central-dir
            0,    0,                // entries on disk
            0,    0,                // total entries
            0,    0,    0,    0,    // central-dir size
            0,    0,    0,    0,    // central-dir offset
            0,    0,
        }; // comment len
        out = std::string(kEocd, sizeof(kEocd));
    } else {
        std::ifstream in(tmp, std::ios::binary | std::ios::ate);
        if (!in) {
            return std::nullopt;
        }
        const std::streamoff fSz = in.tellg();
        if (fSz < 0) {
            return std::nullopt;
        }
        in.seekg(0, std::ios::beg);
        out = std::string(static_cast<std::size_t>(fSz), '\0');
        if (!in.read(out->data(), fSz)) {
            out.reset();
        }
    }
    fs::remove(tmp, ec);
    return out;
}

// Parsed numeric sort key for one (ch-dir, page-file) entry. A value of
// (UINT_MAX, UINT_MAX, "") sorts last and deterministically (design D2):
// an unparsable name is still exported, it just lands at the tail.
struct EntryKey {
    std::uint32_t chapter;
    std::uint32_t page;
    std::string name;
};

// "chNNN" or "chNNN-<id8>" -> NNN. Empty when the dir does not parse.
std::uint32_t ch_num(const std::string& dir) {
    if (dir.size() < 3 || dir.compare(0, 2, "ch") != 0)
        return 0;
    std::string digits = dir.substr(2);
    if (const auto dash = digits.find('-'))
        digits = digits.substr(0, dash);
    if (digits.empty())
        return 0;
    std::size_t used;
    const auto n = std::stoul(digits, &used);
    return used == digits.size() ? static_cast<std::uint32_t>(n) : 0;
}

// "pNNN.<ext>" -> NNN. Empty when the file does not parse.
std::uint32_t page_num(const std::string& file) {
    if (file.size() < 2 || file[0] != 'p')
        return 0;
    std::string digits = file.substr(1, file.find('.', 1) - 1);
    if (digits.empty())
        return 0;
    std::size_t used;
    const auto n = std::stoul(digits, &used);
    return used == digits.size() ? static_cast<std::uint32_t>(n) : 0;
}

} // namespace

std::optional<std::string> build(const std::string& cache_root, const std::string& plugin, const std::string& slug) {
    struct Slot {
        EntryKey key;
        fs::path path;
    };
    std::vector<Slot> slots; // reading order = numeric (ch, page), not string
    if (!plugin.empty() && !slug.empty()) {
        // Layout v3 pages: <root>/manga/<manga>/<ch-dir>/pNNN.ext. Slug carries
        // an optional "/chNNN" suffix exporting a single chapter; prefix match
        // keeps cache.cpp's "-id8" collision variant visible. Cover omitted:
        // v3 cover files are content-hash named, no slug mapping at fs level.
        const auto slash = slug.find('/');
        const std::string manga = slash == std::string::npos ? slug : slug.substr(0, slash);
        const std::string ch_filter = slash == std::string::npos ? "" : slug.substr(slash + 1);
        const fs::path base = fs::path(cache_root) / "img" / plugin / "manga" / cache::slug_dir(manga);
        std::error_code ec;
        for (const auto& ch : fs::directory_iterator(base, ec)) {
            if (ec) {
                break;
            }
            if (!ch.is_directory(ec)) {
                continue;
            }
            const auto name = ch.path().filename().string();
            if (!ch_filter.empty() && !name.starts_with(ch_filter)) {
                continue;
            }
            for (const auto& page : fs::directory_iterator(ch.path(), ec)) {
                if (!ec && page.is_regular_file(ec)) {
                    const auto entry_name = (ch.path().filename() / page.path().filename()).generic_string();
                    const auto ch_dir = ch.path().filename().string();
                    const auto pg_file = page.path().filename().string();
                    const auto cnum = ch_num(ch_dir);
                    const auto pnum = page_num(pg_file);
                    slots.push_back({{cnum ? cnum : UINT32_MAX, pnum ? pnum : UINT32_MAX, entry_name}, page.path()});
                }
            }
        }
    }
    std::sort(slots.begin(), slots.end(), [](const Slot& a, const Slot& b) {
        if (a.key.chapter != b.key.chapter)
            return a.key.chapter < b.key.chapter;
        if (a.key.page != b.key.page)
            return a.key.page < b.key.page;
        return a.key.name < b.key.name; // deterministic tiebreak
    });
    std::vector<std::pair<std::string, fs::path>> entries;
    entries.reserve(slots.size());
    for (const auto& s : slots)
        entries.emplace_back(s.key.name, s.path);
    const auto tmp = fs::temp_directory_path() /
                     ("cbz-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
    return zip_of(tmp, entries);
}

} // namespace cbz
