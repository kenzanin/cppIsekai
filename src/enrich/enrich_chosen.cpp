#include "enrich_internal.hpp"

namespace http::api {

#include "enrich_chosen_internal.hpp"
namespace {
 std::string lower_copy(const std::string& s) {
    std::string out = s;
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
} // namespace

// Merge title_meta.chosen over the scraper detail envelope (design D3):
//   chosen.<field> ?? source; tags = (source tags ∪ chosen.tags_add) − chosen.tags_hidden;
//   cover override is also exposed as `chosen_cover`; `enrichment: {fetched_at, chosen}` rides
//   the top level for the Alpine picker. Corrupt/absent rows return the envelope untouched.
 std::string apply_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                                const std::string& envelope) {
    // One db round-trip for both rows (the chosen row and the name fallback):
    const auto snap = on_pool(db, [&] { return std::pair{store.title_meta_get(plugin, slug), store.saved_get(plugin, slug)}; });
    const auto& row = snap.first;
    // Display-title resolver, shared with the library grid (home_context): the detail
    // name must equal the card name -- chosen title, else saved library name, else the
    // scraper's own title. Also guarantees detail.name for enrichment-less plugins
    // (detail h1 reads it; raw envelopes carry `title` only).
    const auto& saved = snap.second;
    bool renamed = false;
    auto ensure_name = [&](glz::generic& env) {
        if (!env.is_object()) {
            return;
        }
        auto& obj = env.get_object();
        auto* data = obj.find("data");
        if (data == obj.end() || !data->second.is_object()) {
            return;
        }
        auto& d = data->second.get_object();
        if (auto* n = d.find("name"); n != d.end()) {
            if (const auto* s = std::get_if<std::string>(&n->second.data); s != nullptr && !s->empty()) {
                return;
            }
        }
        if (auto* t = d.find("title"); t != d.end()) { // scraper title beats saved_manga.name (library rows can hold the slug)
            if (const auto* s = std::get_if<std::string>(&t->second.data); s != nullptr && !s->empty()) {
                // copy first: glaze's object_t is a flat map, and inserting "name" can move
                // the title node -- assigning through *s then reads freed memory (empty name).
                const std::string title = *s;
                d["name"].data = title;
                renamed = true;
                return;
            }
        }
        if (saved && !saved->name.empty() && saved->name != slug) {
            d["name"].data = saved->name;
            renamed = true;
        }
    };
    glz::generic env;
    if (glz::read_json(env, envelope) || !envelope_ok(env) || !env.is_object()) {
        return envelope;
    }
    glz::generic chosen;
    // no title_meta row or an unusable `chosen`: envelope + the name resolver only
    const bool have_chosen = row.has_value() && !glz::read_json(chosen, row->chosen) && chosen.is_object();
    if (!have_chosen) {
        ensure_name(env);
        // Unchanged envelope: hand the caller the original bytes instead of
        // re-serializing what we just parsed (every detail open hits this).
        return renamed ? glz::write_json(env).value_or(envelope) : envelope;
    }
    auto& obj = env.get_object();
    const auto& cd = chosen.get_object();

    if (auto* data = obj.find("data"); data != obj.end() && data->second.is_object()) {
        auto& d = data->second.get_object();
        const auto set_str = [&](const char* chosen_key, const char* data_key) {
            const auto* const it = cd.find(chosen_key);
            if (it == cd.end()) {
                return;
            }
            const auto* s = std::get_if<std::string>(&it->second.data);
            if (s != nullptr && !s->empty()) {
                d[data_key] = it->second;
            }
        };
        set_str("title", "name");
        set_str("synopsis", "description");
        set_str("cover", "cover");
        const auto set_names = [&](const char* chosen_key, const char* data_key) {
            const auto* const it = cd.find(chosen_key);
            if (it == cd.end()) {
                return;
            }
            if (const auto* s = std::get_if<std::string>(&it->second.data)) {
                if (s->empty()) {
                    return;
                }
                glz::generic::array_t arr;
                arr.emplace_back().data = *s;
                d[data_key].data = std::move(arr);
            } else if (it->second.is_array()) {
                d[data_key] = it->second;
            }
        };
        set_names("artist", "artists");
        set_names("author", "authors");
        if (const auto* const it = cd.find("cover"); it != cd.end()) {
            if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
                d["chosen_cover"] = it->second; // page hydrates from this too
            }
        }

        const auto* const tadd = cd.find("tags_add");
        const auto* const thid = cd.find("tags_hidden");
        if (tadd != cd.end() || thid != cd.end()) {
            std::vector<std::string> tags;
            if (auto* const it = d.find("tags"); it != d.end() && it->second.is_array()) {
                for (const auto& t : it->second.get_array()) {
                    if (const auto* s = std::get_if<std::string>(&t.data)) {
                        tags.push_back(*s);
                    }
                }
            }
            if (tadd != cd.end() && tadd->second.is_array()) {
                for (const auto& t : tadd->second.get_array()) {
                    if (const auto* s = std::get_if<std::string>(&t.data)) {
                        tags.push_back(*s);
                    }
                }
            }
            if (thid != cd.end() && thid->second.is_array()) {
                std::vector<std::string> hidden;
                for (const auto& t : thid->second.get_array()) {
                    if (const auto* s = std::get_if<std::string>(&t.data)) {
                        hidden.push_back(lower_copy(*s));
                    }
                }
                std::erase_if(tags, [&](const std::string& t) {
                    for (const auto& h : hidden) {
                        if (h == lower_copy(t)) {
                            return true;
                        }
                    }
                    return false;
                });
            }
            std::unordered_set<std::string> seen;
            glz::generic::array_t out_tags;
            for (const auto& t : tags) {
                if (seen.insert(lower_copy(t)).second) {
                    glz::generic v;
                    v.data = t;
                    out_tags.push_back(std::move(v));
                }
            }
            d["tags"].data = std::move(out_tags);
        }
    }

    ensure_name(env); // chosen merged; guarantee name even when chosen carries no title
    glz::generic enr;
    auto& e = enr.data.emplace<glz::generic::object_t>();
    e["fetched_at"] = glz::generic{};
    e["fetched_at"].data = static_cast<double>(row->fetched_at);
    e["chosen"] = chosen;
    obj["enrichment"] = std::move(enr);
    return glz::write_json(env).value_or(envelope);
}

} // namespace http::api
