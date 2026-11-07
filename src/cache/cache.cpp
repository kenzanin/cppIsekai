#include "cache/cache.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

} // namespace

Cache::Cache(Config conf, Fetcher fetch) : conf_(std::move(conf)), fetch_(std::move(fetch)) {
    // Boot rebuild (design D1): every file on disk counts toward the cap and
    // keeps serving; mtime order seeds the recency ticks.
    struct Seed {
        std::string path;
        std::int64_t bytes;
        fs::file_time_type mtime;
    };
    std::vector<Seed> seeds;
    std::error_code ec;
    for (const char* kind : {"img", "api"}) {
        const fs::path dir = fs::path(conf_.root) / kind;
        fs::create_directories(dir, ec);
        for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
            const std::string name = e.path().filename().string();
            std::error_code fec;
            if (!e.is_regular_file(fec) || fec) continue;
            if (!name.empty() && name[0] == '.') continue; // ".tmp" leftovers
            seeds.push_back({e.path().string(), static_cast<std::int64_t>(e.file_size(fec)),
                             e.last_write_time(fec)});
        }
    }
    std::stable_sort(seeds.begin(), seeds.end(),
                     [](const Seed& a, const Seed& b) { return a.mtime < b.mtime; });
    for (Seed& s : seeds) {
        bytes_ += s.bytes;
        entries_.emplace(std::move(s.path), Entry{s.bytes, ++tick_});
    }
}

std::optional<std::string> Cache::ensure(const std::string& key, const std::string& kind) {
    // ponytail: one lock across the (blocking) fetch serializes concurrent
    // misses; per-key inflight dedup when parallel workers actually starve.
    std::lock_guard lock(mu_);

    const std::string path =
        (fs::path(conf_.root) / kind / (sha1_hex(key) + "." + ext_of(key))).string();

    if (const auto hit = entries_.find(path); hit != entries_.end()) {
        hit->second.tick = ++tick_; // recency bump on serve
        return path;
    }

    const std::optional<std::string> body = fetch_ ? fetch_(key) : std::nullopt;
    if (!body) return std::nullopt;

    // temp + rename: a crash mid-write never leaves a truncated cache entry
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(body->data(), static_cast<std::streamsize>(body->size()));
        out.close();
        std::error_code ec;
        if (out.fail() || (fs::rename(tmp, path, ec), ec)) {
            fs::remove(tmp, ec);
            return std::nullopt;
        }
    }

    const auto bytes = static_cast<std::int64_t>(body->size());
    bytes_ += bytes;
    entries_.emplace(path, Entry{bytes, ++tick_});
    trim(path);
    return path;
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
