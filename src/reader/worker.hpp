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
 *  concurrent-miss dedup included. Layout v2 (D5): each batch also carries
 *  the window context {plugin, slug, chapter}; when complete (and the url has
 *  an extension) the file lands at <plugin>/<slug>/chNNN/pNNN.<ext> via
 *  rel_path, otherwise the exact old sha fallback. Nothing here touches
 *  routes or state: the queue is the only worker state, newest list wins. */

#include "cache/cache.hpp"
#include "cache/sha1.hpp"

#include <spdlog/spdlog.h>

#include <cctype>
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
 *  force the sha fallback. */
struct WindowCtx {
    std::string plugin;
    std::string slug;
    int chapter{};   // 1-based chapter number; 0 = unknown
    int page_base{}; // chapter-absolute page of batch[0]; 0 = legacy batch position
    std::string chapter_id; // id8 cache-dir suffix; empty keeps the plain chNNN shape
};

/** \brief <plugin>/<slug>/chNNN/pNNN.<ext> from \p ctx + the url's 1-based
 *  window position; empty when any piece is missing -> the exact old sha
 *  path. */
inline std::string rel_path(const WindowCtx& ctx, int page, const std::string& url) {
    if (ctx.plugin.empty() || ctx.slug.empty() || ctx.chapter < 1 || page < 1)
        return {};
    // Ext rule mirrors the sha fallback's ext_of (cache.cpp): trailing
    // dot-suffix of the url path, 1-5 alnum chars, lowercased; absent -> ""
    // which falls back to the exact sha path.
    const std::string path = url.substr(0, url.find('?'));
    const std::string name = path.substr(path.find_last_of('/') + 1);
    const auto dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 == name.size() || name.size() - dot > 6)
        return {};
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) {
        if (!std::isalnum(static_cast<unsigned char>(c)))
            return {};
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const auto pad = [](int n) {
        auto s = std::to_string(n);
        while (s.size() < 3)
            s = "0" + s;
        return s;
    };
    // V3 (cache-layout-v3 D4): <plugin>/manga/<slug>/chNNN/pNNN.<ext> -- same
    // shape as cache page_rel, so worker and /cache/img land one file per
    // slot (ext-free hit, no dupes). The per-chapter id8 suffix for duplicate
    // numbers lives in cache::page_rel via PageCtx.chapter_id; this helper
    // stays on the legacy shape (its caller is the sha-fallback branch).
    return ctx.plugin + "/manga/" + ctx.slug + "/ch" + pad(ctx.chapter) + "/p" + pad(page) + "." + ext;
}

class Worker {
public:
    explicit Worker(cache::Cache& images) : images_(images), thread_([this] { loop(); }) {}
    ~Worker() { stop(); }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    /** \brief Event: queue the freshest window. Coalesces -- a pending list
     *  is replaced, re-run semantics (only the latest position/skip matters).
     *
     *  ctx optional: structured placement when plugin/slug/chapter are known
     *  (2.1), otherwise the exact old sha fallback. */
    void nudge(std::vector<std::string> urls, WindowCtx ctx = {}) {
        {
            const std::lock_guard lock(mu_);
            pending_ = std::move(urls);
            ctx_ = std::move(ctx);
        }
        cv_.notify_one();
    }

    /** \brief Graceful stop: drain whatever is queued, finish the running
     *  batch, join. Idempotent; the destructor calls it. */
    void stop() {
        {
            const std::lock_guard lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable())
            thread_.join();
    }

private:
    void loop() {
        std::unique_lock lock(mu_);
        while (true) {
            cv_.wait(lock, [&] { return stop_ || pending_; });
            if (!pending_)
                return; // stop_ and nothing left to drain
            const std::vector<std::string> batch = std::move(*pending_);
            const WindowCtx ctx = std::move(ctx_);
            ctx_ = {};
            pending_.reset();
            lock.unlock();
            for (std::size_t i = 0; i < batch.size(); ++i) {
                const int page = ctx.page_base > 0 ? ctx.page_base + int(i) : int(i) + 1;
                const cache::Cache::PageCtx pctx{ctx.plugin, ctx.slug, ctx.chapter, page, ctx.chapter_id};
                const auto p =
                    ctx.plugin.empty()
                        ? images_.ensure(
                              batch[i], "img",
                              rel_path(ctx, ctx.page_base > 0 ? ctx.page_base + int(i) : int(i) + 1, batch[i]))
                        : images_.img_ensure(batch[i], {}, pctx); // transcode to [cache] image_format
                spdlog::debug("worker: prefetch {} -> {}", batch[i], p ? *p : std::string("failed"));
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
