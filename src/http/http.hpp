#pragma once
#include "log/log.hpp"
#include "store/store.hpp"

#include <BS_thread_pool.hpp>

void serve(Store& store, BS::thread_pool<>& db, LogBuffer& logs, int port);
