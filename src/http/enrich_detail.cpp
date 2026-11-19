#include "enrich_internal.hpp"

namespace http::api {

// --- manga-detail-table: row materialization + read-first serve ---
// (saved_has_detail lives in store.hpp next to the row it predicates)

/** \brief D2: persist the resolved display fields of a detail envelope onto the saved
 *  row. Called from /api/detail after apply_chosen and from refresh_title when the
 *  detail method ran -- NEVER from apply_chosen itself (the enrichment guard test
 *  asserts candidates never touch saved_manga). Tags are the merged list the page
 *  renders; the display synopsis is `description` falling back to `synopsis`. */
 void materialize_detail(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                               const std::string& slug, const std::string& resolved_env) {
    glz::generic env;
    if (glz::read_json(env, resolved_env) || !envelope_ok(env) || !env.is_object()) {
        return;
    }
    auto* const data = env.get_object().find("data");
    if (data == env.get_object().end() || !data->second.is_object()) {
        return;
    }
    auto& d = data->second.get_object();
    const auto arr_json = [&d](const char* key) {
        auto* const it = d.find(key);
        if (it == d.end() || !it->second.is_array()) {
            return std::string{};
        }
        return glz::write_json(it->second).value_or(std::string{});
    };
    const auto str_of = [&d](const char* key) {
        auto* const it = d.find(key);
        if (it == d.end()) {
            return std::string{};
        }
        const auto* s = std::get_if<std::string>(&it->second.data);
        return s != nullptr ? *s : std::string{};
    };
    std::string syn = str_of("description");
    if (syn.empty()) {
        syn = str_of("synopsis");
    }
    const std::string name = str_of("name");
    const std::string authors = arr_json("authors");
    const std::string artists = arr_json("artists");
    const std::string tags = arr_json("tags");
    on_pool(db, [&] {
        store.saved_detail_put({.plugin = plugin,
                                .slug = slug,
                                .name = name,
                                .authors = authors,
                                .artists = artists,
                                .tags = tags,
                                .synopsis_display = syn,
                                .cover = str_of("cover")});
        return 0;
    });
}

/** \brief D3/D7/D8: the row-served detail envelope for a saved manga -- row values
 *  plus the title_meta overlay (enrichment block + chosen_cover), flagged cached.
 *  Empty string when the row cannot serve (absent or nothing materialized). */
 std::string saved_detail_response(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                         const std::string& slug) {
    const auto saved = on_pool(db, [&] { return store.saved_get(plugin, slug); });
    if (!saved.has_value() || !saved_has_detail(*saved)) {
        return {};
    }
    glz::generic env;
    auto& root = env.data.emplace<glz::generic::object_t>();
    glz::generic data;
    auto& d = data.data.emplace<glz::generic::object_t>();
    const auto set_str = [&d](const char* key, std::string value) { d[key].data = std::move(value); };
    set_str("name", saved->name);
    set_str("cover", saved->cover);
    set_str("status", saved->status);
    set_str("description", !saved->synopsis_display.empty() ? saved->synopsis_display : saved->synopsis); // D4 fallback
    const auto set_arr = [&d](const char* key, const std::string& json) {
        glz::generic arr;
        if (!json.empty() && !glz::read_json(arr, json) && arr.is_array()) {
            d[key] = std::move(arr);
        }
    };
    set_arr("authors", saved->authors);
    set_arr("artists", saved->artists);
    set_arr("tags", saved->tags);
    set_arr("alt_titles", saved->alt_titles); // source verbatim; chosen never touches it

    root["ok"].data = true;
    root["data"] = std::move(data);
    root["cached"].data = true; // D8: the row IS the offline copy

    // D7: title_meta overlay so the picker hydrates exactly as on the live path
    if (const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); }); row.has_value()) {
        glz::generic chosen;
        if (!glz::read_json(chosen, row->chosen) && chosen.is_object()) {
            const auto& cd = chosen.get_object();
            if (const auto* const it = cd.find("cover"); it != cd.end()) {
                if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
                    glz::generic cv;
                    cv.data = *s;
                    root["chosen_cover"] = std::move(cv);
                }
            }
            glz::generic enr;
            auto& e = enr.data.emplace<glz::generic::object_t>();
            e["fetched_at"].data = static_cast<double>(row->fetched_at);
            e["chosen"] = std::move(chosen);
            root["enrichment"] = std::move(enr);
        }
    }
    return glz::write_json(env).value_or(std::string{});
}

} // namespace http::api
