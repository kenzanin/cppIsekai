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

Cache::Cache(Config conf, Fetcher fetch, NsResolver ns)
    : conf_(std::move(conf)), fetch_(std::move(fetch)), ns_(std::move(ns)) {
    // Boot rebuild (design D1): every file on disk counts toward the cap and
    // keeps serving; mtime order seeds the recency ticks. The walk is the whole
    // root tree (D3) -- structured <plugin>/<slug>/... pools index alongside the
    // flat sha fallback with no special casing.
    const fs::path root(conf_.root);

    std::error_code ec;
    for (const char* kind : {"img"}) {
        fs::create_directories(root / kind, ec);
    }
    std::vector<fs::path> stale; // rollback copies left by earlier boots
    for (const fs::directory_entry& e : fs::directory_iterator(root, ec)) {
        if (e.path().filename().string().starts_with(".legacy-")) {
            stale.push_back(e.path());
        }
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
        if (internal::legacy_v1(root)) {
            for (const char* kind : {"img"}) {
                std::error_code rec;
                const fs::path from = root / kind;
                if (fs::exists(from, rec)) {
                    fs::rename(from, root / (std::string(".legacy-") + kind), rec);
                }
            }
        }
        std::ofstream marker(stamp, std::ios::binary); // v2 era begins: never purge again
    }

    // Namespace cutover: <root>/img/<dir> is keyed by the plugin display name
    // (cache::cache_ns) instead of the plugin id, so every existing id-keyed
    // directory is stale by definition. Move the whole img pool aside once --
    // the next boot's sweep above deletes it. A cold cache is the accepted
    // cost; there is no id -> name migration to get wrong. Guarded on ns_:
    // without a resolver the id IS the namespace, so those pools are live.
    const fs::path ns_stamp = root / ".cache-ns-name";
    if (ns_ && !fs::exists(ns_stamp, ec)) {
        std::error_code rec;
        const fs::path from = root / "img";
        if (fs::exists(from, rec)) {
            fs::rename(from, root / ".legacy-img", rec);
        }
        fs::create_directories(root / "img", rec);
        std::ofstream marker(ns_stamp, std::ios::binary);
    }

    // Slug-dir cutover: manga folders are now "<slug40>-<sha1-8>" (uniform
    // bound, collision-safe); pre-v4 folders carry the raw slug. One-shot
    // aside, next boot's .legacy sweep deletes it. A cold image cache is the
    // accepted cost; every file is refetchable and DB rows keep the URLs.
    const fs::path v4_stamp = root / ".cache-v4-slugdir";
    if (!fs::exists(v4_stamp, ec)) {
        std::error_code rec;
        const fs::path from = root / "img";
        if (fs::exists(from, rec)) {
            fs::rename(from, root / ".legacy-img", rec);
        }
        fs::create_directories(root / "img", rec);
        std::ofstream marker(v4_stamp, std::ios::binary);
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
        if (!it->is_regular_file(fec) || fec) {
            continue;
        }
        // ponytail: cover sidecars (<hash>.json next to covers) excluded from
        // LRU byte accounting (D3).
        if (it->path().parent_path().filename() == "cover" && it->path().extension() == ".json") {
            continue;
        }
        seeds.push_back({.path = it->path().string(),
                         .bytes = static_cast<std::int64_t>(it->file_size(fec)),
                         .mtime = it->last_write_time(fec),
                        });
    }
    std::ranges::stable_sort(seeds, [](const Seed& a, const Seed& b) { return a.mtime < b.mtime; });
    for (Seed& s : seeds) {
        bytes_ += s.bytes;
        entries_.emplace(std::move(s.path), Entry{.bytes = s.bytes, .tick = ++tick_});
    }
}

} // namespace cache
