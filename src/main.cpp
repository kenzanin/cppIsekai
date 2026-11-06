#include "http.hpp"
#include "log.hpp"
#include "store.hpp"

#include <BS_thread_pool.hpp>
#include <CLI/CLI.hpp>  // system 2.7.2: flag parsing
#include <SimpleIni.h>  // vendored 4.27: ini, comments kept on rewrite

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

int main(int argc, char** argv) {
    // Default logger wrapped so every spdlog::info call ends up in the queue sink,
    // not just our own line: one crumb added to the smoke main earlier stays useful.
    auto queue_sink = std::make_shared<QueueSink>();
    auto base = spdlog::stdout_color_mt("base");
    base->sinks().push_back(queue_sink);
    spdlog::set_default_logger(base);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");

    std::filesystem::create_directory("data");
    Store store("data/library.db");
    LogBuffer logs("data/cppisekai.log");

    // ui pool is crow's own thread pool. db stays at 1: Store holds one connection.
    // ponytail: fetch pool exists so the layout matches the target, nothing calls it yet.
    BS::thread_pool<> db_pool(1);
    BS::thread_pool<> fetch_pool(2);
    BS::thread_pool<> log_pool(1);
    (void)fetch_pool;

    std::atomic<bool> running{true};
    log_pool.detach_task([&] {
        while (running.load()) {
            logs.drain();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        logs.drain();
    });

    // Precedence: --port > $PORT > [server] port in ini > 8090 (8080 = goisekai's on this host)
    CLI::App cli{"cppIsekai - local manga viewer daemon"};
    int port = -1;  // -1 = unset
    cli.add_option("--port", port, "TCP port")->envname("PORT");
    std::string conf = "data/cppisekai.ini";
    cli.add_option("--config", conf, "INI config; comments survive rewrites (SimpleIni)")->capture_default_str();
    CLI11_PARSE(cli, argc, argv);

    CSimpleIniA ini;  // load error (missing file) just leaves port unset
    if (ini.LoadFile(conf.c_str()) >= 0 && port < 0)
        port = static_cast<int>(ini.GetLongValue("server", "port", -1));
    if (port < 0) port = 8090;

    spdlog::info("cppIsekai listening on :{} (config: {})", port, conf);
    serve(store, db_pool, logs, port);
    running.store(false);
}
