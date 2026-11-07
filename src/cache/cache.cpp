#include "cache/cache.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "cache/sha1.hpp"

namespace fs = std::filesystem;

namespace cache {

namespace {

// Trailing dot-suffix of the key's path, sanitized; "bin" when absent.
std::string ext_of(const std::string& key) {
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

// v1 residue signature (D4): regular files sit directly in img//api/ and every
// one is sha-named; any subdir or foreign name is live v2 layout -- keep it.
bool legacy_v1(const fs::path& root) {
    bool seen = false;
    for (const char* kind : {"img", "api"}) {
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

} // namespace

Cache::Cache(Config conf, Fetcher fetch) : conf_(std::move(conf)), fetch_(std::move(fetch)) {
    // Boot rebuild (design D1): every file on disk counts toward the cap and
    // keeps serving; mtime order seeds the recency ticks. The walk is the whole
    // root tree (D3) -- structured <plugin>/<slug>/... pools index alongside the
    // flat sha fallback with no special casing.
    const fs::path root(conf_.root);

    std::error_code ec;
    for (const char* kind : {"img", "api"}) fs::create_directories(root / kind, ec);
    std::vector<fs::path> stale; // rollback copies left by earlier boots
    for (const fs::directory_entry& e : fs::directory_iterator(root, ec)) {
        if (e.path().filename().string().rfind(".legacy-", 0) == 0) stale.push_back(e.path());
    }
    for (const fs::path& p : stale) {
        std::error_code rec;
        fs::remove_all(p, rec);
    }

    // One-time purge (D4): a pure sha-flat img/api tree is v1 residue. Move it
    // aside under .legacy- (rollback copy); the next boot's sweep deletes it.
    // The stamp keeps this to the first v2 boot -- later sha files are the live
    // context-free fallback, not residue.
    const fs::path stamp = root / ".cache-v2";
    if (!fs::exists(stamp, ec)) {
        if (legacy_v1(root)) {
            for (const char* kind : {"img", "api"}) {
                std::error_code rec;
                const fs::path from = root / kind;
                if (fs::exists(from, rec)) fs::rename(from, root / (std::string(".legacy-") + kind), rec);
            }
        }
        std::ofstream marker(stamp, std::ios::binary); // v2 era begins: never purge again
    }

    struct Seed {
        std::string path;
        std::int64_t bytes;
        fs::file_time_type mtime;
    };
    std::vector<Seed> seeds;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end{};
         !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (!name.empty() && name[0] == '.') { // stamp, .tmp, .legacy copies
            it.disable_recursion_pending();
            continue;
        }
        std::error_code fec;
        if (!it->is_regular_file(fec) || fec) continue;
        seeds.push_back({it->path().string(), static_cast<std::int64_t>(it->file_size(fec)),
                         it->last_write_time(fec)});
    }
    std::stable_sort(seeds.begin(), seeds.end(),
                     [](const Seed& a, const Seed& b) { return a.mtime < b.mtime; });
    for (Seed& s : seeds) {
        bytes_ += s.bytes;
        entries_.emplace(std::move(s.path), Entry{s.bytes, ++tick_});
    }
}

std::string Cache::path_for(const std::string& key, const std::string& kind,
                            const std::string& rel_path) const {
    // D2: rel_path (img only) replaces the sha name; caller guarantees a
    // slug-derived fs-safe subpath. Empty keeps v1 byte-for-byte.
    if (!rel_path.empty() && kind == "img")
        return (fs::path(conf_.root) / kind / rel_path).string();
    return (fs::path(conf_.root) / kind / (sha1_hex(key) + "." + ext_of(key))).string();
}

std::optional<std::string> Cache::persist_locked(const std::string& path, const std::string& body) {
    // temp + rename: a crash mid-write never leaves a truncated cache entry
    const std::string tmp = path + ".tmp";
    {
        std::error_code mec;
        fs::create_directories(fs::path(path).parent_path(), mec); // nested rel_path; exists = no-op
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
        out.close();
        std::error_code ec;
        if (out.fail() || (fs::rename(tmp, path, ec), ec)) {
            fs::remove(tmp, ec);
            return std::nullopt;
        }
    }

    const auto bytes = static_cast<std::int64_t>(body.size());
    if (const auto known = entries_.find(path); known != entries_.end()) {
        bytes_ += bytes - known->second.bytes; // refresh of a live entry
        known->second = Entry{bytes, ++tick_};
    } else {
        bytes_ += bytes;
        entries_.emplace(path, Entry{bytes, ++tick_});
    }
    trim(path);
    return path;
}

std::optional<std::string> Cache::ensure(const std::string& key, const std::string& kind,
                                         const std::string& rel_path) {
    // ponytail: one lock across the (blocking) fetch serializes concurrent
    // misses; per-key inflight dedup when parallel workers actually starve.
    std::lock_guard lock(mu_);

    const std::string path = path_for(key, kind, rel_path);

    if (const auto hit = entries_.find(path); hit != entries_.end()) {
        hit->second.tick = ++tick_; // recency bump on serve
        return path;
    }

    const std::optional<std::string> body = fetch_ ? fetch_(key) : std::nullopt;
    if (!body) return std::nullopt;

    return persist_locked(path, *body);
}

std::optional<std::string> Cache::store(const std::string& key, const std::string& kind,
                                        const std::string& body, const std::string& rel_path) {
    const std::lock_guard lock(mu_);
    return persist_locked(path_for(key, kind, rel_path), body);
}

std::optional<std::string> Cache::peek(const std::string& key, const std::string& kind,
                                       const std::string& rel_path) {
    const std::lock_guard lock(mu_);

    const auto it = entries_.find(path_for(key, kind, rel_path));
    if (it == entries_.end()) return std::nullopt;

    std::ifstream in(it->first, std::ios::binary);
    if (!in) {
        // File gone under a live index (external wipe): drop the phantom.
        bytes_ -= it->second.bytes;
        entries_.erase(it);
        return std::nullopt;
    }
    std::string body{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    it->second.tick = ++tick_; // same recency bump as an ensure() hit
    return body;
}

void Cache::trim(const std::string& keep) {
    while (bytes_ > conf_.max_bytes) {
        auto victim = entries_.end();
        std::uint64_t oldest = ~0ULL;
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->first != keep && it->second.tick < oldest) {
                oldest = it->second.tick;
                victim = it;
            }
        }
        if (victim == entries_.end()) break; // only `keep` left: cap < one file
        bytes_ -= victim->second.bytes;
        std::error_code ec;
        fs::remove(victim->first, ec);
        entries_.erase(victim);
    }
}

} // namespace cache
