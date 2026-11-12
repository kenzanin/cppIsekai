#pragma once

/** \file cover_worker.hpp
 *  \brief Background cover prefetcher: ONE thread, woken by the library /
 *  grid render with the saved-manga cover urls. Cache-first: img_ensure is
 *  the delta filter (disk hit = no fetch), so re-renders cost one stat
 *  each. Covers land via the v3 cover ctx (<plugin>/cover/<hash>-<r4>.<ext>),
 *  same path /cache/img?cover=1 serves. Queue coalesces -- newest list wins. */

#include "cache/cache.hpp"

#include <spdlog/spdlog.h>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace reader {

struct CoverJob {
    std::string url;
    std::string plugin;
};

class CoverWorker {
public:
    explicit CoverWorker(cache::Cache& images) : images_(images), thread_([this] { loop(); }) {}
    ~CoverWorker() { stop(); }

    CoverWorker(const CoverWorker&) = delete;
    CoverWorker& operator=(const CoverWorker&) = delete;

    void nudge(std::vector<CoverJob> jobs) {
        {
            const std::scoped_lock lock(mu_);
            pending_ = std::move(jobs);
        }
        cv_.notify_one();
    }

    void stop() {
        {
            const std::scoped_lock lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void loop() {
        std::unique_lock lock(mu_);
        while (true) {
            cv_.wait(lock, [&] { return stop_ || pending_; });
            if (!pending_) {
                return; // stop_ and nothing left to drain
            }
            const std::vector<CoverJob> batch = std::move(*pending_);
            pending_.reset();
            lock.unlock();
            for (const auto& j : batch) {
                cache::Cache::CoverCtx ctx;
                ctx.cover = true;
                ctx.plugin = j.plugin;
                ctx.url = j.url;
                const auto p = images_.img_ensure(j.url, ctx, {});
                spdlog::debug("cover-worker: prefetch {} -> {}", j.url, p ? *p : std::string("failed"));
            }
            lock.lock();
        }
    }

    cache::Cache& images_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<std::vector<CoverJob>> pending_;
    bool stop_ = false;
    std::thread thread_;
};

} // namespace reader
