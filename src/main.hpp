#pragma once

#include "cache/slug.hpp"
#include "config/config.hpp"
#include "http/http.hpp"
#include "browser/chrome.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"
#include "update/scheduler.hpp"

#include <CLI/CLI.hpp> // system 2.7.2: flag parsing
#include <BS_thread_pool.hpp>
#include <backward.hpp> // xrepo, header-only profile: crash-trace handler

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
