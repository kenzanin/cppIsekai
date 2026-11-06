#pragma once
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <BS_thread_pool.hpp>

void serve(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
           LogBuffer& logs, int port);
