#pragma once

/** \file cache.hpp
 *  \brief Disk cache core: data/cache/<kind>/<sha1(key)>.<ext>, or layout v2's
 *  data/cache/<kind>/<rel_path>; ONE shared LRU pool over the whole tree.
 *
 * Index is in-memory {bytes, tick}; rebuilt on boot from a recursive walk with
 * mtime order as the initial recency, so cache presence survives restart. The
 * first v2 boot purges legacy flat sha pools (D4: .legacy- rename, next boot
 * sweeps it). Recency bumps on every serve; evict-on-insert caps the pool at
 * Config::max_bytes (ini [cache] cache_max_mb, default 2048 -- wired where the
 * host constructs the cache).
 *
 * Layout v3 (cache-layout-v3 D1-D4): img pool supports context-aware write via
 * img_store(cover_ctx / page_ctx) producing <plugin>/covers/<hash>.<ext> and
 * <plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext>. Extension-free lookup (by
 * content-addressed base) serves any stored variant. Sidecars (hash.json) carry
 * decoded dims+ratio for cover serves; they sit outside LRU byte accounting.
 *
 * Fetcher and root are injectable (binding interface): unit tests run
 * hermetic on a temp dir + fake fetcher, the production fetcher attaches
 CURLOPT_PROXY, and the worker reuses this same primitive. */

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace cache {

/** \brief Body for key -- an URL for the img pool, an opaque key
 *  ("plugin|method|q") for the api pool. nullopt = failure (offline,
 *  transport error). */
using Fetcher = std::function<std::optional<std::string>(const std::string& key)>;

struct Config {
    std::string root = "data/cache";
    std::int64_t max_bytes = 2048LL << 20; // cache_max_mb, default 2048
    std::string image_format = "webp";     // [cache] image_format: webp|avif|jxl|none
    std::int64_t cover_max_px = 800;       // [cache] cover_max_px: cover resize cap
};

class Cache {
public:
    /** \brief Walks the whole root tree: existing files keep serving and count
     *  toward the cap. */
    Cache(Config conf, Fetcher fetch);

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    /** \brief Hit: return path, bump recency. Miss: fetch -> store -> path.
     *
     *  kind is the subpool directory ("img"). Optional rel_path (D2,
     *  img only) lands the file at <root>/<kind>/<rel_path> -- caller passes
     *  the full slug-derived subpath incl. filename; empty keeps the v1
     *  <sha><ext> name. The index stays keyed by on-disk path, so a second
     *  rel for one key is a second live copy, refetched on demand. nullopt
     *  when the fetch fails; the caller decides (404, stale, ...). */
    std::optional<std::string> ensure(const std::string& key, const std::string& kind,
                                      const std::string& rel_path = {});

    // --- stale layer (task 2.2, additive; ensure/Fetcher untouched) ---

    /** \brief Persist body for key under kind -- the write half of ensure(),
     *  no fetch. A repeat store refreshes the entry and bumps recency like a
     *  hit. Returns the entry path; nullopt on write failure. */
    std::optional<std::string> store(const std::string& key, const std::string& kind, const std::string& body,
                                     const std::string& rel_path = {});

    /** \brief Cached body for key; recency bumps exactly like an ensure() hit.
     *  nullopt on miss, or when the file vanished (index self-heals). */
    std::optional<std::string> peek(const std::string& key, const std::string& kind, const std::string& rel_path = {});

    /** \brief V3 context-aware store for img pool only: writes body under a
     *  layout-specific path (cover / chapter-dir / sha fallback), plus a one-line
     *  JSON sidecar for covers when dims+ratio can be decoded. Returns the stored
     *  path; nullopt on write failure. Decode fail falls back to v2 hash name and
     *  stores original bytes (no sidecar). */
    struct CoverCtx {
        std::string url{};
        std::string plugin{};
        int w{}, h{};
        double ratio{};
        bool cover{};
    };
    struct PageCtx {
        std::string plugin{};
        std::string slug{};
        int chapter{};
        int page{};
    };
    std::optional<std::string> img_store(const std::string& key, const std::string& body, const CoverCtx& cover_ctx,
                                         const PageCtx& page_ctx, const std::string& ext_override = {});

    /** \brief Serve-through ensure for /cache/img (v3 wiring): hit serves
     *  extension-free (cover scan / page prefix scan, any stored format);
     *  miss fetches, transcodes to the configured [cache] image_format
     *  (covers also resized to cover_max_px), stores via the img_store
     *  layout + sidecar. nullopt on fetch/write failure. */
    std::optional<std::string> img_ensure(const std::string& key, const CoverCtx& cover_ctx, const PageCtx& page_ctx);

    /** \brief Extension-free cover lookup (D1): scan the boot index for
     *  img/<plugin>/covers/<sha1(url)-16>.<ext>; old formats keep serving.
     *  nullopt when absent. */
    std::optional<std::string> cover_serve(const std::string& plugin, const std::string& url);

    /** \brief Ratio from a cover filename tail "<hash>-<r4>.<ext>"
     *  (fixed-point 4 digits, no dot in the name besides the extension);
     *  0.0 when absent/malformed — caller omits the X-Image-Ratio header. */
    static double ratio_of_name(const std::string& cover_name);

private:
    std::string path_for(const std::string& key, const std::string& kind, const std::string& rel_path) const;

    /** \brief tmp+rename write + index bookkeeping + trim; mu_ held by
     *  callers. */
    std::optional<std::string> persist_locked(const std::string& path, const std::string& body);

    struct Entry {
        std::int64_t bytes{};
        std::uint64_t tick{};
    };

    /** \brief Evict least-recently-used whole files while over the cap; the
     *  entry just inserted (keep) always survives so the caller gets a real
     *  file back. */
    void trim(const std::string& keep);

    Config conf_;
    Fetcher fetch_;
    std::unordered_map<std::string, Entry> entries_; // by file path
    std::int64_t bytes_{};
    std::uint64_t tick_{}; // recency clock; disk seed = mtime order
    std::mutex mu_;
};

} // namespace cache
