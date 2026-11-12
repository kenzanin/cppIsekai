#pragma once
/** \file scheduler.hpp
 *  \brief auto-update scheduler (auto-update 4.1): one dedicated thread,
 *  60s tick, reads the [update] knobs fresh from the registry each tick
 *  (settings edits take effect without restart), launches a bounded
 *  chapter-list-only refresh sweep when due. */
#include <atomic>
#include <string>
#include <thread>

#include <BS_thread_pool.hpp>

namespace config {
struct Overrides;
}
namespace plugin {
class Host;
}
class Store;

namespace update {

class Scheduler {
public:
    Scheduler(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
              std::string ini_path, const config::Overrides& overrides);
    ~Scheduler(); // stop + join; no detached thread

    void start();
    void stop();
    /** \brief One due-check/sweep, synchronously. Exposed for tests and
     *  scratch verification ("manually triggered run"). */
    void run_once();

private:
    void loop();

    Store& store_;
    BS::thread_pool<>& db_;
    BS::thread_pool<>& plugin_pool_;
    plugin::Host& plugins_;
    std::string ini_path_;
    const config::Overrides& overrides_;

    std::atomic<bool> stop_{false};
    std::thread thread_;
    int jitter_seconds_{0}; // drawn once at construction; keeps sweeps off the exact :00
};

} // namespace update
