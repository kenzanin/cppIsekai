#pragma once

/** \file worker.hpp
 *  \brief Background prefetch worker (image-cache 3.3): ONE thread, no
 *  timer -- it only ever wakes on a nudge from POST /reader/position or
 *  /chapter/<id>/skip.
 *
 *  The endpoint has already re-run the window against the new state; it hands
 *  the resulting urls over and the worker walks them in order through
 *  cache::ensure, which IS the delta filter (hit: on disk, no fetch; miss:
 *  one fetch) -- sequential, on the shared img pool with /cache/img,
 *  concurrent-miss dedup included. Each batch must carry the window context
 *  {plugin, slug, chapter}: the file lands under the plugin namespace at
 *  <ns>/manga/<slug>/chNNN/pNNN.<ext>. A batch without that context is
 *  dropped -- the img pool has no context-free path. Nothing here touches
 *  routes or state: the queue is the only worker state, newest list wins. */

#include "cache/cache.hpp"
#include "cache/sha1.hpp"

#include <spdlog/spdlog.h>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace reader {

/** \brief Placement context for one window batch: the reader knows plugin,
 *  slug (D5) and the chapter number the window belongs to; the page number is
 *  the 1-based position of the url within the batch -- or, when page_base is
 *  set, chapter-absolute (batch[0] is page_base), matching the /cache/img
 *  query-page convention so both paths land one file per slot. Empty fields
 *  mean the batch is dropped: there is no sha fallback to write it to. */
struct WindowCtx {
    std::string plugin;
    std::string slug;
    int chapter{};          // 1-based chapter number; 0 = unknown
    int page_base{};        // chapter-absolute page of batch[0]; 0 = legacy batch position
    std::string chapter_id; // id8 cache-dir suffix; empty keeps the plain chNNN shape
};

class Worker {
public:
    explicit Worker(cache::Cache& images) : images_(images), thread_([this] { loop(); }) {}
    ~Worker() { stop(); }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    /** \brief Event: queue the freshest window. Coalesces -- a pending list
     *  is replaced, re-run semantics (only the latest position/skip matters).
     *
     *  ctx: placement for the batch. A nudge without plugin/slug/chapter is
     *  dropped by the loop -- the img pool has no context-free path. */
    void nudge(std::vector<std::string> urls, WindowCtx ctx = {}) {
        {
            const std::scoped_lock lock(mu_);
            pending_ = std::move(urls);
            ctx_ = std::move(ctx);
        }
        cv_.notify_one();
    }

    /** \brief Graceful stop: drain whatever is queued, finish the running
     *  batch, join. Idempotent; the destructor calls it. */
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
            const std::vector<std::string> batch = std::move(*pending_);
            const WindowCtx ctx = std::move(ctx_);
            ctx_ = {};
            pending_.reset();
            lock.unlock();
            // No plugin/slug/chapter = no placement path; the img pool has no
            // context-free slot, so drop the batch instead of writing junk.
            if (!ctx.plugin.empty() && !ctx.slug.empty() && ctx.chapter >= 1) {
                for (std::size_t i = 0; i < batch.size(); ++i) {
                    const int page = ctx.page_base > 0 ? ctx.page_base + static_cast<int>(i) : static_cast<int>(i) + 1;
                    const cache::Cache::PageCtx pctx{
                        .plugin = ctx.plugin,
                        .slug = ctx.slug,
                        .chapter = ctx.chapter,
                        .page = page,
                        .chapter_id = ctx.chapter_id,
                    };
                    const auto p = images_.img_ensure(batch[i], {} pctx); // transcode to [cache] image_format
                    spdlog::debug("worker: prefetch {} -> {}", batch[i], p ? *p : std::string("failed"));
                }
            }
            lock.lock();
        }
    }

    cache::Cache& images_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<std::vector<std::string>> pending_;
    WindowCtx ctx_;
    bool stop_ = false;
    std::thread thread_; // last: touches every member above once running
};

} // namespace reader
