#include "cache/cache.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "cache/sha1.hpp"
#include "cache/transcode.hpp"

#include <spdlog/spdlog.h>

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

// --- layout v3 (cache-layout-v3 D1-D4) helpers ---

// V3 layout helpers: cover / chapter dir / hash fallback rel_path builders.
// Cover ratio rides in the filename as fixed-point 4 digits (no dot):
// <hash>-6988.webp == ratio 0.6988. Sidecar .json files are gone.
std::string cover_rel(const std::string& plugin, const std::string& url_hash,
                      const std::string& ext, double ratio) {
    const int r4 = ratio > 0.0 ? static_cast<int>(std::lround(ratio * 10000.0)) : 0;
    return plugin + "/cover/" + url_hash + "-" + std::to_string(r4) + "." + ext;
}

/** \brief Ratio from a cover filename tail "<hash>-<r4>.<ext>"; 0.0 when
 *  absent/zero (old files without the suffix keep serving, no header). */
double ratio_from_name(const std::string& name) {
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

std::string page_rel(const std::string& plugin, const std::string& slug, int chapter, int page) {
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

// (sidecar writer removed: ratio now rides in the cover filename)

} // namespace

Cache::Cache(Config conf, Fetcher fetch) : conf_(std::move(conf)), fetch_(std::move(fetch)) {
    // Boot rebuild (design D1): every file on disk counts toward the cap and
    // keeps serving; mtime order seeds the recency ticks. The walk is the whole
    // root tree (D3) -- structured <plugin>/<slug>/... pools index alongside the
    // flat sha fallback with no special casing.
    const fs::path root(conf_.root);

    std::error_code ec;
    for (const char* kind : {"img"}) fs::create_directories(root / kind, ec);
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
            for (const char* kind : {"img"}) {
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
        // ponytail: cover sidecars (<hash>.json next to covers) excluded from
        // LRU byte accounting (D3).
        if (it->path().parent_path().filename() == "cover" &&
            it->path().extension() == ".json")
            continue;
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

std::optional<std::string> Cache::img_store(const std::string& key, const std::string& body,
                                            const CoverCtx& cover_ctx, const PageCtx& page_ctx,
                                            const std::string& ext_override) {
    // V3 layout (D1-D4): cover -> <plugin>/cover/<urlhash16>.<ext> + sidecar;
    // page -> <plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext> via page_rel;
    // no context -> sha fallback. ext_override (transcode target ext) wins,
    // else extension derived from the key.
    const std::string ext = ext_override.empty() ? ext_of(key) : ext_override;
    std::string rel;
    if (cover_ctx.cover) {
        const auto h = sha1_hex(cover_ctx.url.empty() ? key : cover_ctx.url).substr(0, 16);
        rel = cover_rel(cover_ctx.plugin.empty() ? "common" : cover_ctx.plugin, h, ext,
                    cover_ctx.w > 0 && cover_ctx.h > 0
                        ? static_cast<double>(cover_ctx.w) / static_cast<double>(cover_ctx.h)
                        : 0.0);
    } else if (page_ctx.plugin.empty() || page_ctx.slug.empty() || page_ctx.chapter < 1 ||
               page_ctx.page < 1) {
        rel = {}; // no complete page context: sha fallback
    } else {
        rel = page_rel(page_ctx.plugin, page_ctx.slug, page_ctx.chapter, page_ctx.page) +
              "." + ext;
    }

    const std::lock_guard lock(mu_);
    const std::string path = rel.empty() ? path_for(key, "img", {})
                                         : (fs::path(conf_.root) / "img" / rel).string();
    auto stored = persist_locked(path, body);
    if (!stored) return std::nullopt;
    return stored;
}

std::optional<std::string> Cache::img_ensure(const std::string& key, const CoverCtx& cover_ctx,
                                             const PageCtx& page_ctx) {
    // Hit: extension-free lookup, any stored format serves (mid-flight
    // format switch, D2). Miss: fetch -> transcode -> img_store layout.
    // ponytail: lock/unlock per section — cover_serve takes mu_ itself,
    // so it must run outside this lock.
    // Stale-hit guard: entries_ is boot-scan in-memory; a file deleted under
    // us (manual cache wipe) would otherwise serve a 200-empty-body blank
    // instead of falling through to fetch. ponytail: exists() per hit costs
    // one stat; the real fix is FS-watch invalidation, add if wipes get hot.
    // ponytail: exists()/erase run OUTSIDE mu_ — alive() is never called
    // under the lock (std::mutex would deadlock); erase is idempotent.
    const auto alive = [this](const std::string& p) {
        if (fs::exists(p))
            return true;
        const std::lock_guard lock(mu_);
        entries_.erase(p);
        spdlog::debug("cache: img stale index entry dropped {}", p);
        return false;
    };

    std::optional<std::string> hit; // candidate chosen under lock, verified outside
    {
        const std::lock_guard lock(mu_);
        if (!cover_ctx.cover && !page_ctx.plugin.empty() && !page_ctx.slug.empty() &&
            page_ctx.chapter >= 1 && page_ctx.page >= 1) {
            const std::string prefix =
                page_rel(page_ctx.plugin, page_ctx.slug, page_ctx.chapter, page_ctx.page) + ".";
            for (const auto& [path, _] : entries_)
                if (path.find(prefix) != std::string::npos && !path.ends_with(".json")) {
                    hit = path;
                    break;
                }
            if (!hit)
                spdlog::debug("cache: img ctx-miss, fetching {} (ch{} p{} {})", key,
                              page_ctx.chapter, page_ctx.page, page_ctx.slug);
            // Context miss: fetch below lands in this exact v3 spot. Deliberately
            // no sha-fallback lookup here -- an entry there predates this
            // context's layout and would serve the wrong landing path.
        } else {
            // No (or incomplete) context: sha-fallback hit check.
            const std::string fb = path_for(key, "img", {});
            if (entries_.count(fb))
                hit = fb;
            else
                spdlog::debug("cache: img sha-miss, fetching {}", key);
        }
    }
    if (hit && !alive(*hit))
        hit.reset(); // deleted under us: fall through to fetch
    if (hit) {
        const std::lock_guard lock(mu_);
        auto it = entries_.find(*hit);
        if (it != entries_.end()) it->second.tick = ++tick_; // recency bump
        spdlog::debug("cache: img hit {} ({})", *hit, key);
        return *hit;
    }
    if (cover_ctx.cover) {
        auto hit = cover_serve(cover_ctx.plugin, cover_ctx.url.empty() ? key : cover_ctx.url);
        if (hit && alive(*hit)) {
            const std::lock_guard lock(mu_);
            auto it = entries_.find(*hit);
            if (it != entries_.end()) it->second.tick = ++tick_;
            return hit;
        }
        spdlog::debug("cache: img cover-miss, fetching {} (plugin={})", key, cover_ctx.plugin);
    }

    const std::optional<std::string> raw = fetch_ ? fetch_(key) : std::nullopt;
    if (!raw) {
        spdlog::debug("cache: img fetch failed {} (cover={} plugin={} slug={} ch{} p{})", key,
                      cover_ctx.cover, cover_ctx.plugin, page_ctx.slug, page_ctx.chapter,
                      page_ctx.page);
        return std::nullopt;
    }

    const auto tc = transcode(*raw, conf_.image_format,
                                         cover_ctx.cover ? static_cast<int>(conf_.cover_max_px) : 0);
    if (tc.bytes.empty()) {
        spdlog::debug("cache: img transcode failed {} ({} B raw)", key, raw->size());
        return std::nullopt;
    }
    CoverCtx ctx = cover_ctx;
    if (tc.width > 0) { // decoded: host dims are source of truth (D3)
        ctx.w = tc.width;
        ctx.h = tc.height;
    }
    auto stored = img_store(key, tc.bytes, ctx, page_ctx, tc.ext);
    if (stored) spdlog::debug("cache: img stored {} ({} B)", *stored, tc.bytes.size());
    return stored;
}

std::optional<std::string> Cache::cover_serve(const std::string& plugin,
                                              const std::string& url) {
    // Extension-free lookup (D1): entries_ keys are full on-disk paths
    // (<root>/img/...); covers land at <root>/img/<plugin>/cover/<sha1(url)-16>-<r4>.<ext>.
    // Match the tail (hash prefix, any ratio suffix and ext).
    const std::lock_guard lock(mu_);
    const std::string tail = "img/" +
                             (plugin.empty() ? std::string("common") : plugin) +
                             "/cover/" + sha1_hex(url).substr(0, 16) + "-";
    for (const auto& [path, _] : entries_) {
        if (path.find(tail) != std::string::npos)
            return path;
    }
    return std::nullopt;
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

// Cache::ratio_of_name — public wrapper over the anonymous-namespace helper.
double Cache::ratio_of_name(const std::string& cover_name) { return ratio_from_name(cover_name); }

} // namespace cache
