#include "plugin_internal.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

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

namespace {

// Host part of a URL (scheme optional); no scheme -> everything before '/'.
std::string url_host(std::string_view u) {
    std::size_t b = 0;
    if (const auto s = u.find("://"); s != std::string_view::npos) {
        b = s + 3;
    }
    const auto stop = std::min({u.find('/', b), u.find('?', b), u.find('#', b)});
    return std::string(u.substr(b, stop == std::string_view::npos ? u.size() - b : stop - b));
}

// Same registrable site: exact, or one is a subdomain of the other.
bool same_site(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    if (a.size() > b.size() && a.ends_with("." + b)) return true;
    return b.size() > a.size() && b.ends_with("." + a);
}

} // namespace

std::string Host::image_referer_for_url(const std::string& url, const std::string& fallback_id) const {
    const std::string host = url_host(url);
    std::vector<std::string> ids;
    if (!host.empty()) {
        const std::scoped_lock lock(impl_->mu);
        for (const auto& [id, rt] : impl_->plugins) {
            const auto& info = rt->info;
            if (same_site(host, url_host(info.website)) ||
                same_site(host, url_host(info.image_referer))) {
                ids.push_back(id);
            }
        }
    }
    for (const auto& id : ids) {
        if (std::string ref = image_referer(id); !ref.empty()) {
            return ref; // "none" sentinel is honored by the caller
        }
    }
    return image_referer(fallback_id);
}

std::string Host::image_engine(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    return it != impl_->plugins.end() ? it->second->info.image_engine : std::string{};
}

std::string Host::image_ua(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    return it != impl_->plugins.end() ? it->second->info.image_ua : std::string{};
}

std::int64_t Host::cache_ttl_seconds(const std::string& id, const std::string& method) const {
    const std::scoped_lock lock(impl_->mu);
    const auto it = impl_->plugins.find(id);
    if (it == impl_->plugins.end()) {
        return 0;
    }
    const auto& table = it->second->info.cache_ttl;
    const auto t = table.find(method);
    return t != table.end() ? t->second : 0;
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

bool Host::loaded(const std::string& id) const {
    const std::scoped_lock lock(impl_->mu);
    return impl_->plugins.contains(id);
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
