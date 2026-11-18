#include "enrich_internal.hpp"

namespace http::api {

// Serve the per-source entry of the stored `fetched` array shaped as the
// enrichment envelope; nullopt when this source has nothing stored.
 std::optional<crow::json::wvalue> stored_source(const std::string& fetched_json, const std::string& id,
                                                       const std::string& kind, std::int64_t& out_fetched_at) {
    glz::generic fetched;
    if (glz::read_json(fetched, fetched_json) || !fetched.is_array()) {
        return std::nullopt;
    }
    for (const auto& entry : fetched.get_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto& obj = entry.get_object();
        const auto* const src = obj.find("source");
        if (src == obj.end()) {
            continue;
        }
        const auto* s = std::get_if<std::string>(&src->second.data);
        if (s == nullptr || *s != id) {
            continue;
        }
        // entries are keyed (source, kind); a missing kind is a legacy detail entry
        const auto* const k = obj.find("kind");
        const auto* ks = k != obj.end() ? std::get_if<std::string>(&k->second.data) : nullptr;
        if (((ks != nullptr) ? *ks : std::string("detail")) != kind) {
            continue;
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["cached"] = true;
        out["source"] = id;
        const auto* const at = obj.find("fetched_at");
        double at_d = 0;
        if (at != obj.end()) {
            if (const auto* d = std::get_if<double>(&at->second.data)) {
                at_d = *d;
            }
        }
        out_fetched_at = static_cast<std::int64_t>(at_d);
        out["fetched_at"] = at_d;
        const auto* const data = obj.find("data");
        out["data"] = data != obj.end() ? crow::json::load(glz::write_json(data->second).value_or(std::string{}))
                                        : crow::json::wvalue();
        return out;
    }
    return std::nullopt;
}



// Validate + normalize the POST chosen member into a compact JSON string.
// nullopt on any shape violation (caller returns 400). Exact key whitelist;
// artist/author accept String or List<String> and normalize to array.
 std::optional<std::string> normalize_chosen(const crow::json::rvalue& ch) {
    if (ch.t() != crow::json::type::Object) {
        return std::nullopt;
    }
    static const char* allowed[] = {
        "title", "synopsis", "artist", "author", "cover", "tags_add", "tags_hidden", "related_add",
    };
    for (const auto& member : ch) {
        const std::string k = std::string(member.key());
        bool ok = false;
        for (const char* a : allowed) {
            if (k == a) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            return std::nullopt;
        }
    }
    crow::json::wvalue out;
    for (const char* key : {"title", "synopsis", "cover"}) {
        if (ch.has(key) && ch[key].t() == crow::json::type::String) {
            out[key] = ch[key];
        }
    }
    for (const char* key : {"artist", "author"}) {
        if (!ch.has(key)) {
            continue;
        }
        if (ch[key].t() == crow::json::type::String) {
            crow::json::wvalue one(crow::json::type::List);
            one[0] = ch[key];
            out[key] = std::move(one);
        } else if (ch[key].t() == crow::json::type::List) {
            crow::json::wvalue arr(crow::json::type::List);
            int n = 0;
            for (const auto& item : ch[key]) {
                if (item.t() != crow::json::type::String) {
                    return std::nullopt;
                }
                arr[n++] = item;
            }
            out[key] = std::move(arr);
        } else {
            {
                return std::nullopt;
            }
        }
    }
    if (ch.has("related_add")) {
        const auto& arr = ch["related_add"];
        if (arr.t() != crow::json::type::List) {
            return std::nullopt;
        }
        crow::json::wvalue out_arr(crow::json::type::List);
        int n = 0;
        for (const auto& item : arr) {
            if (item.t() != crow::json::type::Object) {
                return std::nullopt;
            }
            // extra keys rejected; name/url required non-empty, id optional non-empty string
            for (const auto& member : item) {
                const std::string k = std::string(member.key());
                if (k != "name" && k != "url" && k != "id") {
                    return std::nullopt;
                }
            }
            if (!item.has("name") || !item.has("url")) {
                return std::nullopt;
            }
            crow::json::wvalue row;
            for (const char* key : {"name", "url", "id"}) {
                if (!item.has(key)) {
                    continue;
                }
                if (item[key].t() != crow::json::type::String) {
                    return std::nullopt;
                }
                const std::string v = item[key].s();
                if (v.empty() || v.size() > 512) {
                    return std::nullopt;
                }
                row[key] = v;
            }
            out_arr[n++] = std::move(row);
        }
        out["related_add"] = std::move(out_arr);
    }
    for (const char* key : {"tags_add", "tags_hidden"}) {
        if (!ch.has(key)) {
            continue;
        }
        const auto& arr = ch[key];
        if (arr.t() != crow::json::type::List) {
            return std::nullopt;
        }
        crow::json::wvalue out_arr(crow::json::type::List);
        int n = 0;
        for (const auto& item : arr) {
            if (item.t() != crow::json::type::String) {
                return std::nullopt;
            }
            out_arr[n++] = item;
        }
        out[key] = std::move(out_arr);
    }
    return std::string(out.dump());
}

// Chosen display title from title_meta.chosen; nullopt when unset/empty.
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
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    // Display-title resolver, shared with the library grid (home_context): the detail
    // name must equal the card name -- chosen title, else saved library name, else the
    // scraper's own title. Also guarantees detail.name for enrichment-less plugins
    // (detail h1 reads it; raw envelopes carry `title` only).
    const auto saved = on_pool(db, [&] { return store.saved_get(plugin, slug); });
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
                return;
            }
        }
        if (saved && !saved->name.empty() && saved->name != slug) {
            d["name"].data = saved->name;
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
        return glz::write_json(env).value_or(envelope);
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
