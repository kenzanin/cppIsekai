#include "export_cbz.hpp"

#include <libzippp.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>

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

std::optional<std::string> zip_of(const fs::path& tmp, const std::map<std::string, fs::path>& entries) {
    std::optional<std::string> out;
    {
        libzippp::ZipArchive zip(tmp.string());
        if (!zip.open(libzippp::ZipArchive::New))
            return std::nullopt;
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
        constexpr char kEocd[22] = {0x50, 0x4b, 0x05, 0x06, // sig
                                    0,    0,                // disk num
                                    0,    0,                // disk w/central-dir
                                    0,    0,                // entries on disk
                                    0,    0,                // total entries
                                    0,    0,    0,    0,    // central-dir size
                                    0,    0,    0,    0,    // central-dir offset
                                    0,    0};               // comment len
        out = std::string(kEocd, sizeof(kEocd));
    } else {
        std::ifstream in(tmp, std::ios::binary | std::ios::ate);
        if (!in)
            return std::nullopt;
        const std::streamoff fSz = in.tellg();
        if (fSz < 0)
            return std::nullopt;
        in.seekg(0, std::ios::beg);
        out = std::string(static_cast<std::size_t>(fSz), '\0');
        if (!in.read(out->data(), fSz)) {
            out.reset();
        }
    }
    fs::remove(tmp, ec);
    return out;
}

} // namespace

std::optional<std::string> build(const std::string& cache_root, const std::string& plugin, const std::string& slug) {
    std::map<std::string, fs::path> entries; // map = lexicographic entry order
    if (!plugin.empty() && !slug.empty()) {
        // Layout v3 pages: <root>/manga/<manga>/<ch-dir>/pNNN.ext. Slug carries
        // an optional "/chNNN" suffix exporting a single chapter; prefix match
        // keeps cache.cpp's "-id8" collision variant visible. Cover omitted:
        // v3 cover files are content-hash named, no slug mapping at fs level.
        const auto slash = slug.find('/');
        const std::string manga = slash == std::string::npos ? slug : slug.substr(0, slash);
        const std::string ch_filter = slash == std::string::npos ? "" : slug.substr(slash + 1);
        const fs::path base = fs::path(cache_root) / "img" / plugin / "manga" / manga;
        std::error_code ec;
        for (const auto& ch : fs::directory_iterator(base, ec)) {
            if (ec)
                break;
            if (!ch.is_directory(ec))
                continue;
            const auto name = ch.path().filename().string();
            if (!ch_filter.empty() && name.rfind(ch_filter, 0) != 0)
                continue;
            for (const auto& page : fs::directory_iterator(ch.path(), ec)) {
                if (!ec && page.is_regular_file(ec))
                    entries.emplace((ch.path().filename() / page.path().filename()).generic_string(), page.path());
            }
        }
    }
    const auto tmp = fs::temp_directory_path() /
                     ("cbz-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
    return zip_of(tmp, entries);
}

} // namespace cbz
