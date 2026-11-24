#include "plugin_internal.hpp"
#include "store/store.hpp"

#include <filesystem>
#include <functional>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace plugin {

int Host::load_all(Store& store) {
    std::map<std::string, std::shared_ptr<Runtime>> fresh;

    // Storage closures: same gate for every runtime this scan produces.
    Store* sp = &store;
    BS::thread_pool<>* dbp = impl_->db;
    StateGet sget = [sp, dbp](const std::string& pid, const std::string& key) -> std::optional<std::string> {
        if (dbp != nullptr) {
            return dbp->submit_task([&] { return sp->state_get(pid, key); }).get();
        }
        return sp->state_get(pid, key); // single-threaded tests
    };
    StateSet sset = [sp, dbp](const std::string& pid, const std::string& key, const std::string& val) {
        if (dbp != nullptr) {
            dbp->submit_task([&] { sp->state_set(pid, key, val); }).get();
            return;
        }
        sp->state_set(pid, key, val);
    };

    scan_dir(conf_, impl_->live_profile, store, sget, sset, conf_.dir, false, fresh);
    if (!conf_.enrichment_dir.empty()) {
        scan_dir(conf_, impl_->live_profile, store, sget, sset, conf_.enrichment_dir, true, fresh);
    }

    // folder-removal GC (plugin-lifecycle 2.2): registry ids absent from this
    // scan are purged. A missing scan dir short-circuits the prune so a mount
    // glitch cannot wipe the library; an emptied dir legitimately does.
    if (std::filesystem::exists(conf_.dir)) {
        std::vector<std::string> keep;
        keep.reserve(fresh.size());
        for (const auto& entry : fresh)
            keep.push_back(entry.first);
        for (const auto& gone : store.plugin_prune_absent(keep))
            spdlog::info("[plugin] {}: folder gone, registry + traces purged", gone);
    }

    const std::scoped_lock lock(impl_->mu);
    const int total = static_cast<int>(fresh.size());
    impl_->plugins = std::move(fresh);
    return total;
}

} // namespace plugin
