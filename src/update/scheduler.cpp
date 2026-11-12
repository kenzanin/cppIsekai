#include "update/scheduler.hpp"

#include "config/config.hpp"
#include "http/api_ctx.hpp"
#include "plugin/plugin.hpp"
#include "store/store.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <random>
#include <spdlog/spdlog.h>

namespace {
constexpr const char* kLastRunKey = "update/last_run";
constexpr const char* kStateHost = "host";
} // namespace

namespace update {

Scheduler::Scheduler(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool, plugin::Host& plugins,
                     std::string ini_path, const config::Overrides& overrides)
    : store_(store), db_(db), plugin_pool_(plugin_pool), plugins_(plugins), ini_path_(std::move(ini_path)),
      overrides_(overrides) {
    jitter_seconds_ = std::rand() % (15 * 60 + 1); // <=15 min; scheduler is advisory, not precise
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    spdlog::info("[auto-update] scheduler started (60s tick)");
    thread_ = std::thread([this] { loop(); });
}

void Scheduler::stop() {
    stop_.store(true);
    if (thread_.joinable())
        thread_.join();
}

void Scheduler::loop() {
    while (!stop_.load()) {
        run_once();
        for (int i = 0; i < 60 && !stop_.load(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Scheduler::run_once() {
    // knobs re-read per tick: /settings edits take effect without a restart
    const config::Config cfg = config::resolve(overrides_, ini_path_);
    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));

    const std::int64_t interval_s = static_cast<std::int64_t>(cfg.update_interval_days) * 86400;
    const std::optional<std::string> last =
        http::on_pool(db_, [&] { return store_.state_get(kStateHost, kLastRunKey); });
    std::int64_t last_run = 0;
    if (last && !last->empty())
        last_run = std::strtoll(last->c_str(), nullptr, 10);
    if (last_run != 0 && now - last_run < interval_s - jitter_seconds_)
        return; // not due yet

    auto batch = http::on_pool(db_, [&] { return store_.update_candidates(now, cfg.update_per_plugin_limit); });
    if (batch.empty()) {
        spdlog::info("[auto-update] run: 0 eligible titles");
        http::on_pool(db_, [&] {
            store_.state_set(kStateHost, kLastRunKey, std::to_string(now));
            return 0;
        });
        return;
    }

    int ok = 0, err = 0;
    for (const auto& t : batch) {
        const RefreshOutcome rr =
            refresh_title(store_, db_, plugin_pool_, plugins_, t.plugin, t.slug, /*chapters_only=*/true);
        if (rr.result == RefreshResult::kOk)
            ++ok;
        else
            ++err;
        // pacing between titles, not after the last
        if (&t != &batch.back() && cfg.update_request_delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.update_request_delay_ms));
    }
    http::on_pool(db_, [&] {
        store_.state_set(kStateHost, kLastRunKey, std::to_string(now));
        return 0;
    });
    spdlog::info("[auto-update] run: checked={} ok={} error={}", batch.size(), ok, err);
}

} // namespace update
