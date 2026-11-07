#pragma once
#include "cache/cache.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <BS_thread_pool.hpp>

// Production fetcher for cache misses: one curl GET, body on 2xx else nullopt.
// Single choke point by design -- task 2.1 attaches CURLOPT_PROXY here and in
// the plugin fetch; keep it that way.
cache::Fetcher makeCurlFetcher();

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
           LogBuffer& logs, cache::Cache& images, int port);
