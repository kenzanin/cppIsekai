#pragma once

/** \file http.hpp
 *  \brief HTTP layer surface: the single cache fetcher and serve(). */

#include "cache/cache.hpp"
#include "config/config.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <BS_thread_pool.hpp>

/** \brief Production fetcher for cache misses: one curl GET, body on 2xx,
 *  else nullopt. Single choke point by design -- proxy (ini [server] proxy)
 *  rides on every handle via the fetch gate (src/fetch); keep it that way. */
cache::Fetcher makeCurlFetcher(const std::string& proxy);

/** \brief Composition root: registers every route on a fresh crow app and
 *  blocks in run() until shutdown. */
void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins, LogBuffer& logs,
           cache::Cache& images, int port, const std::string& ini_path, const config::Overrides& overrides,
           const std::string& templates_dir);
