#include "enrich_internal.hpp"

namespace http::api {

 std::optional<std::string> chosen_title(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                               const std::string& slug) {
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!row) {
        return std::nullopt;
    }
    glz::generic chosen;
    if (glz::read_json(chosen, row->chosen) || !chosen.is_object()) {
        return std::nullopt;
    }
    auto* const it = chosen.get_object().find("title");
    if (it == chosen.get_object().end()) {
        return std::nullopt;
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
        return *s;
    }
    return std::nullopt;
}

 std::optional<std::string> chosen_cover(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                               const std::string& slug) {
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!row) {
        return std::nullopt;
    }
    glz::generic chosen;
    if (glz::read_json(chosen, row->chosen) || !chosen.is_object()) {
        return std::nullopt;
    }
    const auto& cd = chosen.get_object();
    const auto* const it = cd.find("cover");
    if (it == cd.end()) {
        return std::nullopt;
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
        return *s;
    }
    return std::nullopt;
}

} // namespace http::api
