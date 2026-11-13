#include "plugin_internal.hpp"

namespace fs = std::filesystem;

namespace plugin {

Host::Host(Config conf) : conf_(conf), impl_(std::make_unique<Host::Impl>()) {
    impl_->conf = std::move(conf);
    impl_->live_profile = std::make_shared<const std::string>(conf_.fetch_profile);
}

Host::~Host() = default;

void Host::use_db(BS::thread_pool<>* db) {
    impl_->db = db;
}

void Host::set_default_profile(std::string profile) {
    impl_->live_profile = std::make_shared<const std::string>(std::move(profile));
}

std::string Host::default_profile() const {
    if (auto p = impl_->live_profile.load()) {
        return *p;
    }
    return conf_.fetch_profile;
}

CallHealth Host::health(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->health.find(id);
    return it != impl_->health.end() ? it->second : CallHealth{};
}

std::string Host::website(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    return it != impl_->plugins.end() ? it->second->info.website : std::string{};
}

std::string Host::image_referer(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    if (it == impl_->plugins.end()) {
        return {};
    }
    if (!it->second->info.image_referer.empty()) {
        return it->second->info.image_referer;
    }
    // Default: the origin (scheme://host) of the manifest website.
    const std::string& site = it->second->info.website;
    const auto scheme = site.find("://");
    if (scheme == std::string::npos) {
        return site;
    }
    const auto path = site.find('/', scheme + 3);
    return path == std::string::npos ? site : site.substr(0, path);
}

std::string Host::logo_path(const std::string& id) const {
    std::shared_ptr<Runtime> rt;
    {
        const std::scoped_lock lock(impl_->mu);
        if (const auto it = impl_->plugins.find(id); it != impl_->plugins.end()) {
            rt = it->second;
        }
    }
    if (!rt) {
        return "";
    }
    for (const char* name : {"logo.png", "logo.svg", "logo.jpg", "logo.jpeg", "logo.avif"}) {
        const fs::path p = fs::path(rt->info.dir) / name;
        if (fs::exists(p)) {
            return p.string();
        }
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> Host::enrichment_sources() const {
    const std::scoped_lock lock(impl_->mu);
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& [id, rt] : impl_->plugins) {
        const auto& caps = rt->info.capabilities;
        for (const auto& c : caps) {
            if (c == "enrichment") {
                out.emplace_back(id, rt->info.name);
                break;
            }
        }
    }
    return out;
}

std::vector<Host::EnrichmentRow> Host::enrichment_rows() const {
    const std::scoped_lock lock(impl_->mu);
    std::vector<EnrichmentRow> out;
    for (const auto& [id, rt] : impl_->plugins) {
        const auto& info = rt->info;
        bool is_enr = false;
        std::string caps;
        for (size_t i = 0; i < info.capabilities.size(); ++i) {
            if (info.capabilities[i] == "enrichment")
                is_enr = true;
            caps += (i != 0 ? "," : "") + info.capabilities[i];
        }
        if (!is_enr)
            continue;
        EnrichmentRow row;
        row.id = info.id;
        row.name = info.name;
        row.version = info.version;
        row.runtime = info.runtime;
        row.caps = caps;
        row.website = info.website;
        if (const auto it = impl_->health.find(id); it != impl_->health.end()) {
            row.ok = it->second.ok;
            row.error = it->second.error;
            row.checked_at = it->second.at;
        }
        out.push_back(std::move(row));
    }
    return out;
}

bool Host::is_enrichment(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    if (it == impl_->plugins.end())
        return false;
    for (const auto& c : it->second->info.capabilities) {
        if (c == "enrichment")
            return true;
    }
    return false;
}

} // namespace plugin
