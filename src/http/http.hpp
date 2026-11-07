#pragma once
#include "cache/cache.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <BS_thread_pool.hpp>

// Production fetcher for cache misses: one curl GET, body on 2xx else nullopt.
// Single choke point by design -- proxy (ini [server] proxy) rides on every
// handle via plugin::applyProxy; keep it that way.
cache::Fetcher makeCurlFetcher(std::string proxy);

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
           LogBuffer& logs, cache::Cache& images, int port);
