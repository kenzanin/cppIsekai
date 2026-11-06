#pragma once
#include "log.hpp"
#include "store.hpp"

#include <BS_thread_pool.hpp>

void serve(Store& store, BS::thread_pool<>& db, LogBuffer& logs, int port);
