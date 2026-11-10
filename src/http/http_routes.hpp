#pragma once

/** \file http_routes.hpp
 *  \brief Internal route-registration seams (design D3, change source-split-clangdoc).
 *
 * serve() stays the composition root: it builds the app, the prefetch worker
 * and then delegates to the two registration seams below. Helpers the seams
 * share live here as inline glue; page/api-private helpers stay in their
 * translation units.
 */

#include <crow.h>

#include <BS_thread_pool.hpp>

#include <spdlog/spdlog.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <cstdlib>

#include "cache/cache.hpp"
#include "log/log.hpp"
#include "reader/worker.hpp"
#include "reader/cover_worker.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

namespace http {

/** \brief One hop onto a worker pool, caller blocks on the future.
 *
 * Every database call runs on the single db worker, which owns the one
 * connection. Same wrapper serves the plugin pool. Rethrows worker
 * exceptions on the caller thread after logging. */
template <class Fn> auto on_pool(BS::thread_pool<>& pool, Fn fn) -> decltype(fn()) {
    auto done = std::make_shared<std::promise<decltype(fn())>>();
    pool.detach_task([done, fn = std::move(fn)] {
        try {
            done->set_value(fn());
        } catch (...) {
            done->set_exception(std::current_exception()); // release the waiting future either way
        }
    });
    try {
        return done->get_future().get();
    } catch (const std::exception& e) {
        spdlog::error("on_pool failed: {}", e.what());
        throw;
    }
}

/** \brief Query parameter or empty string. */
inline std::string param(const crow::request& req, const char* key) {
    const char* value = req.url_params.get(key);
    return (value != nullptr) ? value : "";
}

/** \brief 1-based page number from a query value. */
inline int page_of(const std::string& text) {
    const int page = text.empty() ? 1 : std::atoi(text.c_str());
    return page < 1 ? 1 : page;
}

/** \brief HTML pages + the log websocket: /, /library/grid, /search,
 *  /history, /plugins, /about, /logs, ws /log. */
void register_pages(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& logs, reader::CoverWorker& covers);

/** \brief JSON API: plugin registry/call routes, reader position/skip,
 *  config save, image cache, CBZ export, dev reload. */
void register_api(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                  plugin::Host& plugins, cache::Cache& images, reader::Worker& prefetch, reader::CoverWorker& covers, const std::string& ini_path);

} // namespace http
