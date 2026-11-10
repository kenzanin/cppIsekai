#include "config/config.hpp"
#include "http/http.hpp"
#include "log/log.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <CLI/CLI.hpp> // system 2.7.2: flag parsing
#include <BS_thread_pool.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    // Default logger wrapped so spdlog::info call ends up in queue sink,
    // not just our own line: one crumb added smoke main earlier stays useful.
    auto queue_sink = std::make_shared<QueueSink>();
    auto base = spdlog::stdout_color_mt("base");
    base->sinks().push_back(queue_sink);
    spdlog::set_default_logger(base);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%l] %v");

    // Every knob comes from the one registry (src/config/); layered here by
    // config::resolve: default < ini < CPPISEKAI_* env < explicit flag (D7).
    config::Overrides overrides;
    CLI::App cli{"cppIsekai - local manga viewer daemon"};
    std::string ini_path = "data/cppisekai.ini"; // help-text default; boot itself requires --config (D3)
    auto* cfg_opt = cli.add_option("--config", ini_path, "INI config; comments survive rewrites (SimpleIni)")
                        ->capture_default_str();
    auto* dump_opt = cli.add_flag("--dump-config", "print ini template (doc comment + default per knob), then exit");
    std::string dump_path; // positional next to --dump-config: target file, created/truncated
    cli.add_option("path", dump_path, "with --dump-config: write the template here instead of stdout");
    config::build_cli(cli, overrides);
    CLI11_PARSE(cli, argc, argv); // --help / bad flags print and exit here

    if (dump_opt->count() > 0) { // D3/D4: exempt from required --config, never boots
        const std::string text = config::dump();
        if (dump_path.empty()) {
            std::cout << text;
        } else {
            std::ofstream out{dump_path, std::ios::trunc}; // existing configs untouched
            if (!out) {
                std::cerr << "cppisekai: cannot write " << dump_path << '\n';
                return 1;
            }
            out << text;
        }
        return 0;
    }

    if (cfg_opt->count() == 0) { // D3: manual required-check, not CLI11 required()
        std::cout << "usage: cppisekai --config <file.ini> [options]\n"
                     "try 'cppisekai --help' for every option.\n";
        return 2; // no boot: DB untouched, no data/ created
    }
    const config::Config cfg = config::resolve(overrides, ini_path);
    // Boot-time level from the registry (log.level); bad values keep info.
    {
        const auto lvl = spdlog::level::from_str(cfg.log_level);
        spdlog::set_level(lvl == spdlog::level::off && cfg.log_level != "off" ? spdlog::level::info : lvl);
    }

    std::filesystem::create_directories(cfg.data_dir); // [paths] data_dir, boot-created (D6)
    Store store(cfg.db);
    LogBuffer logs(cfg.log_file);

    // db pool stays 1:1, Store holds one connection; crow runs its own pool.
    // ponytail: db_pool exists for the storage.* layout target, nothing queued yet.
    BS::thread_pool<> db_pool(1);
    BS::thread_pool<> plugin_pool(2); // blocking http.fetch plugin calls live here
    BS::thread_pool<> log_pool(1);

    std::atomic<bool> running{true};
    log_pool.detach_task([&] {
        while (running.load()) {
            logs.drain();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        logs.drain();
    });

    // [plugin]: discovery dir and http.fetch limits; resolved values logged for ops.
    plugin::Config plugin_conf;
    plugin_conf.dir = cfg.plugins_dir;
    plugin_conf.fetch_timeout_ms = cfg.fetch_timeout_ms;
    plugin_conf.fetch_profile = cfg.fetch_profile;
    plugin_conf.instruction_budget = cfg.plugin_timeout_ops;
    plugin_conf.proxy = cfg.proxy;
    spdlog::info("plugins dir={} fetch_timeout_ms={} fetch_profile={} ops={}", //
                 plugin_conf.dir, plugin_conf.fetch_timeout_ms, plugin_conf.fetch_profile,
                 plugin_conf.instruction_budget);

    plugin::Host plugins{plugin_conf};
    plugins.use_db(&db_pool); // storage.* gate: plugin threads never touch sqlite directly
    plugins.load_all(store);

    // [cache]: one shared disk-LRU (img now, api in 3.3); cap resolved in MiB.
    const std::int64_t cache_max_bytes = cfg.cache_max_mb << 20;
    cache::Config ccfg{"data/cache", cache_max_bytes};
    ccfg.image_format = cfg.image_format;   // ponytail: must ride Config, else ini knob silently ignored
    ccfg.cover_max_px = cfg.cover_max_px;
    cache::Cache images{ccfg, makeCurlFetcher(cfg.proxy)};

    spdlog::info("cppIsekai listening on :{} (config: {})", cfg.port, ini_path);
    serve(store, db_pool, plugin_pool, plugins, logs, images, cfg.port, ini_path, overrides, cfg.templates_dir);
    running.store(false);
}
