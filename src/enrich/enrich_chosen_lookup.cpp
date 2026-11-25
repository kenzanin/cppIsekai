#include "enrich_internal.hpp"

namespace http::api {

#include "enrich_chosen_internal.hpp"

std::optional<std::string> chosen_field(const glz::generic& chosen, const char* key) {
    if (!chosen.is_object()) {
        return std::nullopt;
    }
    const auto& obj = chosen.get_object();
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return std::nullopt;
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
        return *s;
    }
    return std::nullopt;
}

namespace {

std::optional<std::string> stored_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                         const std::string& slug, const char* key) {
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!row) {
        return std::nullopt;
    }
    glz::generic chosen;
    if (glz::read_json(chosen, row->chosen) || !chosen.is_object()) {
        return std::nullopt;
    }
    return chosen_field(chosen, key);
}

} // namespace

std::optional<std::string> chosen_title(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug) {
    return stored_chosen(store, db, plugin, slug, "title");
}

std::optional<std::string> chosen_cover(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug) {
    return stored_chosen(store, db, plugin, slug, "cover");
}

} // namespace http::api
