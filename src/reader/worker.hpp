#pragma once

/** \file worker.hpp
 *  \brief Background prefetch worker (image-cache 3.3): ONE thread, no
 *  timer -- it only ever wakes on a nudge from POST /reader/position or
 *  /chapter/<id>/skip.
 *
 *  The endpoint has already re-run the window against the new state; it hands
 *  the resulting per-chapter parts over and the worker walks them in order
 *  through cache::ensure, which IS the delta filter (hit: on disk, no fetch;
 *  miss: one fetch) -- sequential, on the shared img pool with /cache/img,
 *  concurrent-miss dedup included. Each part carries its own chapter id plus
 *  number and first page, so a window spilled into the next chapter lands in
 *  that chapter's own dir at its real page -- never as chapter N page N+1.
 *  A nudge without plugin/slug is dropped -- the img pool has no
 *  context-free path. Nothing here touches routes or state: the queue is the
 *  only worker state, newest list wins. */

#include "cache/cache.hpp"
#include "reader/window.hpp"

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
     *  parts: placement per chapter slice (window_parts). Empty plugin/slug
     *  or empty parts drops the batch: no placement path, no sha fallback. */
    void nudge(std::vector<WindowPart> parts, std::string plugin, std::string slug) {
        {
            const std::scoped_lock lock(mu_);
            pending_ = std::move(parts);
            plugin_ = std::move(plugin);
            slug_ = std::move(slug);
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
            const std::vector<WindowPart> batch = std::move(*pending_);
            const std::string plugin = std::move(plugin_);
            const std::string slug = std::move(slug_);
            plugin_.clear();
            slug_.clear();
            pending_.reset();
            lock.unlock();
            // No plugin/slug = no placement path; the img pool has no
            // context-free slot, so drop the batch instead of writing junk.
            // Per-part chapter/first-page: each url lands in its OWN chapter
            // dir at its real page (spill included), matching the path the
            // reader's /cache/img request will hit.
            if (!plugin.empty() && !slug.empty()) {
                for (const auto& part : batch) {
                    const int chapter = static_cast<int>(part.number);
                    if (chapter < 1 || part.first_page < 1) {
                        continue; // unnumbered chapter: no trustworthy dir
                    }
                    for (std::size_t i = 0; i < part.urls.size(); ++i) {
                        const cache::Cache::PageCtx pctx{
                            .plugin = plugin,
                            .slug = slug,
                            .chapter = chapter,
                            .page = part.first_page + static_cast<int>(i),
                            .chapter_id = part.chapter_id
                        };
                        const auto p = images_.img_ensure(part.urls[i], {}, pctx); // transcode to [cache] image_format
                        spdlog::debug("worker: prefetch {} -> {}", part.urls[i], p ? *p : std::string("failed"));
                    }
                }
            }
            lock.lock();
        }
    }

    cache::Cache& images_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<std::vector<WindowPart>> pending_;
    std::string plugin_;
    std::string slug_;
    bool stop_ = false;
    std::thread thread_; // last: touches every member above once running
};

} // namespace reader
