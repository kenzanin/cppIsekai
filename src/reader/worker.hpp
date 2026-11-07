#pragma once
// Background prefetch worker (image-cache 3.3): ONE thread, no timer -- it
// only ever wakes on a nudge from POST /reader/position or /chapter/<id>/skip.
// The endpoint has already re-run the window against the new state; it hands
// the resulting urls over and the worker walks them in order through
// cache::ensure, which IS the delta filter (hit: on disk, no fetch; miss: one
// fetch) -- sequential, on the shared img pool with /cache/img, concurrent-
// miss dedup included. Nothing here touches routes or state: the queue is the
// only worker state, newest list wins.
#include "cache/cache.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace reader {

class Worker {
public:
    explicit Worker(cache::Cache& images) : images_(images), thread_([this] { loop(); }) {}
    ~Worker() { stop(); }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Event: queue the freshest window. Coalesces -- a pending list is
    // replaced, re-run semantics (only the latest position/skip matters).
    void nudge(std::vector<std::string> urls) {
        {
            const std::lock_guard lock(mu_);
            pending_ = std::move(urls);
        }
        cv_.notify_one();
    }

    // Graceful stop: drain whatever is queued, finish the running batch, join.
    // Idempotent; the destructor calls it.
    void stop() {
        {
            const std::lock_guard lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

private:
    void loop() {
        std::unique_lock lock(mu_);
        while (true) {
            cv_.wait(lock, [&] { return stop_ || pending_; });
            if (!pending_)
                return; // stop_ and nothing left to drain
            const std::vector<std::string> batch = std::move(*pending_);
            pending_.reset();
            lock.unlock();
            for (const auto& url : batch) images_.ensure(url, "img");
            lock.lock();
        }
    }

    cache::Cache& images_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<std::vector<std::string>> pending_;
    bool stop_ = false;
    std::thread thread_; // last: touches every member above once running
};

} // namespace reader
