#pragma once
// Disk cache core: data/cache/<kind>/<sha1(key)>.<ext>, ONE shared LRU pool.
// Index is in-memory {bytes, tick}; rebuilt on boot from a disk scan with
// mtime order as the initial recency, so cache presence survives restart.
// Recency bumps on every serve; evict-on-insert caps the pool at
// Config::max_bytes (ini [cache] cache_max_mb, default 2048 -- wired where the
// host constructs the cache).
// Fetcher and root are injectable (binding interface): unit tests run hermetic
// on a temp dir + fake fetcher, task 2.1 attaches CURLOPT_PROXY on the
// production fetcher, 3.3's worker reuses this same primitive.
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace cache {

// Body for key -- an URL for the img pool, an opaque key ("plugin|method|q")
// for the api pool. nullopt = failure (offline, transport error).
using Fetcher = std::function<std::optional<std::string>(const std::string& key)>;

struct Config {
    std::string root = "data/cache";
    std::int64_t max_bytes = 2048LL << 20; // cache_max_mb, default 2048
};

class Cache {
public:
    // Scans root/{img,api}: existing files keep serving and count toward the cap.
    Cache(Config conf, Fetcher fetch);

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    // Hit: return path, bump recency. Miss: fetch -> store -> path.
    // kind is the subpool directory ("img", "api").
    // nullopt when the fetch fails; the caller decides (404, stale, ...).
    std::optional<std::string> ensure(const std::string& key, const std::string& kind);

private:
    struct Entry {
        std::int64_t bytes{};
        std::uint64_t tick{};
    };

    // Evict least-recently-used whole files while over the cap; the entry just
    // inserted (keep) always survives so the caller gets a real file back.
    void trim(const std::string& keep);

    Config conf_;
    Fetcher fetch_;
    std::unordered_map<std::string, Entry> entries_; // by file path
    std::int64_t bytes_{};
    std::uint64_t tick_{}; // recency clock; disk seed = mtime order
    std::mutex mu_;
};

} // namespace cache
