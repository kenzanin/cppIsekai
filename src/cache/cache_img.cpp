#include "cache/cache.hpp"

#include "cache_internal.hpp"

#include "cache/sha1.hpp"
#include "cache/transcode.hpp"

#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace cache {

std::optional<std::string> Cache::img_store(const std::string& key, const std::string& body,
                                            const CoverCtx& cover_ctx, const PageCtx& page_ctx,
                                            const std::string& ext_override) {
    // V3 layout (D1-D4): cover -> <plugin>/cover/<urlhash16>.<ext> + sidecar;
    // page -> <plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext> via page_rel;
    // no context -> sha fallback. ext_override (transcode target ext) wins,
    // else extension derived from the key.
    const std::string ext = ext_override.empty() ? internal::ext_of(key) : ext_override;
    std::string rel;
    if (cover_ctx.cover) {
        const auto h = sha1_hex(cover_ctx.url.empty() ? key : cover_ctx.url).substr(0, 16);
        rel = internal::cover_rel(cover_ctx.plugin.empty() ? "common" : cover_ctx.plugin, h, ext,
                    cover_ctx.w > 0 && cover_ctx.h > 0
                        ? static_cast<double>(cover_ctx.w) / static_cast<double>(cover_ctx.h)
                        : 0.0);
    } else if (page_ctx.plugin.empty() || page_ctx.slug.empty() || page_ctx.chapter < 1 ||
               page_ctx.page < 1) {
        rel = {}; // no complete page context: sha fallback
    } else {
        rel = internal::page_rel(page_ctx.plugin, page_ctx.slug, page_ctx.chapter, page_ctx.page) +
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
                internal::page_rel(page_ctx.plugin, page_ctx.slug, page_ctx.chapter, page_ctx.page) + ".";
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


} // namespace cache
