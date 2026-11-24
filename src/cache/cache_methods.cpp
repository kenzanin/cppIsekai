#include "cache/cache.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "cache/sha1.hpp"

#include <spdlog/spdlog.h>

#include "cache_internal.hpp"

namespace fs = std::filesystem;

namespace cache {

std::string Cache::path_for(const std::string& key, const std::string& kind, const std::string& rel_path) const {
    // D2: rel_path (img only) names the file; caller guarantees a slug-derived
    // fs-safe subpath. The img pool has no sha fallback (D5): a caller without
    // a placement path gets "" and stores nothing. The api pool keeps its v1
    // sha naming.
    if (!rel_path.empty() && kind == "img") {
        return (fs::path(conf_.root) / kind / rel_path).string();
    }
    if (kind == "img") {
        return {};
    }
    return (fs::path(conf_.root) / kind / (sha1_hex(key) + "." + internal::ext_of(key))).string();
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
        known->second = Entry{.bytes = bytes, .tick = ++tick_};
    } else {
        bytes_ += bytes;
        entries_.emplace(path, Entry{.bytes = bytes, .tick = ++tick_});
    }
    trim(path);
    return path;
}

std::string Cache::ns(const std::string& plugin) const {
    if (!ns_ || plugin.empty()) {
        return plugin;
    }
    std::string out = ns_(plugin);
    return out.empty() ? plugin : out;
}

std::size_t Cache::purge_pair(const std::string& plugin, const std::string& slug,
                              const std::vector<std::string>& cover_urls) {
    std::scoped_lock lock(mu_);
    const fs::path img = fs::path(conf_.root) / "img";
    const std::string manga_dir = (img / ns(plugin) / "manga" / slug_dir(slug)).string();
    // cover_url -> sha16 file prefix; matches every ratio variant of that url
    std::vector<std::string> cover_prefixes;
    cover_prefixes.reserve(cover_urls.size());
    for (const auto& url : cover_urls) {
        if (!url.empty()) {
            cover_prefixes.push_back((img / ns(plugin) / "cover" / sha1_hex(url).substr(0, 16)).string());
        }
    }
    std::size_t removed = 0;
    std::int64_t freed = 0;
    std::error_code ec;
    for (auto it = entries_.begin(); it != entries_.end();) {
        const std::string& path = it->first;
        bool owned = path.rfind(manga_dir, 0) == 0;
        for (const auto& pre : cover_prefixes) {
            owned = owned || path.rfind(pre, 0) == 0;
        }
        if (!owned) {
            ++it;
            continue;
        }
        fs::remove(path, ec);
        freed += it->second.bytes;
        it = entries_.erase(it);
        ++removed;
    }
    fs::remove_all(manga_dir, ec); // empty page-tree dirs
    bytes_ -= freed;
    return removed;
}

std::optional<std::string> Cache::ensure(const std::string& key, const std::string& kind, const std::string& rel_path) {
    // ponytail: one lock across the (blocking) fetch serializes concurrent
    // misses; per-key inflight dedup when parallel workers actually starve.
    std::scoped_lock lock(mu_);

    std::string path = path_for(key, kind, rel_path);
    if (path.empty()) {
        return std::nullopt; // no placement path (img pool, D5): nothing to store
    }

    if (const auto hit = entries_.find(path); hit != entries_.end()) {
        hit->second.tick = ++tick_; // recency bump on serve
        return path;
    }

    const std::optional<std::string> body = fetch_ ? fetch_(key, {}) : std::nullopt;
    if (!body) {
        return std::nullopt;
    }

    return persist_locked(path, *body);
}

std::optional<std::string> Cache::store(const std::string& key, const std::string& kind, const std::string& body,
                                        const std::string& rel_path) {
    const std::scoped_lock lock(mu_);
    const std::string path = path_for(key, kind, rel_path);
    return path.empty() ? std::nullopt : persist_locked(path, body);
}

std::optional<std::string> Cache::peek(const std::string& key, const std::string& kind, const std::string& rel_path) {
    const std::scoped_lock lock(mu_);

    const auto it = entries_.find(path_for(key, kind, rel_path));
    if (it == entries_.end()) {
        return std::nullopt;
    }

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
        if (victim == entries_.end()) {
            break; // only `keep` left: cap < one file
        }
        bytes_ -= victim->second.bytes;
        std::error_code ec;
        fs::remove(victim->first, ec);
        entries_.erase(victim);
    }
}

// Cache::ratio_of_name — public wrapper over the anonymous-namespace helper.
double Cache::ratio_of_name(const std::string& cover_name) {
    return internal::ratio_from_name(cover_name);
}


} // namespace cache
