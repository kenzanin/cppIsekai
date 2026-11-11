#pragma once

/** \file enrich_internal.hpp
 *  \brief Helpers for the enrichment HTTP routes (http_api_enrich.cpp); inline
 *  header so the route TU stays small. Not public API. */

#include "api_ctx.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "store/store.hpp"
#include "plugin/plugin.hpp"

#include <glaze/glaze.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace http {
namespace api {

// Serve the per-source entry of the stored `fetched` array shaped as the
// enrichment envelope; nullopt when this source has nothing stored.
inline std::optional<crow::json::wvalue> stored_source(const std::string& fetched_json,
                                                      const std::string& id,
                                                      const std::string& kind,
                                                      std::int64_t& out_fetched_at) {
    glz::generic fetched;
    if (glz::read_json(fetched, fetched_json) || !fetched.is_array()) {
        return std::nullopt;
    }
    for (const auto& entry : fetched.get_array()) {
        if (!entry.is_object()) continue;
        const auto& obj = entry.get_object();
        const auto src = obj.find("source");
        if (src == obj.end()) continue;
        const auto* s = std::get_if<std::string>(&src->second.data);
        if (s == nullptr || *s != id) continue;
        // entries are keyed (source, kind); a missing kind is a legacy detail entry
        const auto k = obj.find("kind");
        const auto* ks = k != obj.end() ? std::get_if<std::string>(&k->second.data) : nullptr;
        if ((ks ? *ks : std::string("detail")) != kind) continue;
        crow::json::wvalue out;
        out["ok"] = true;
        out["cached"] = true;
        out["source"] = id;
        const auto at = obj.find("fetched_at");
        double at_d = 0;
        if (at != obj.end()) {
            if (const auto* d = std::get_if<double>(&at->second.data)) at_d = *d;
        }
        out_fetched_at = static_cast<std::int64_t>(at_d);
        out["fetched_at"] = at_d;
        const auto data = obj.find("data");
        out["data"] = data != obj.end()
                          ? crow::json::load(glz::write_json(data->second).value_or(std::string{}))
                          : crow::json::wvalue();
        return out;
    }
    return std::nullopt;
}

// Validate + normalize the POST chosen member into a compact JSON string.
// nullopt on any shape violation (caller returns 400). Exact key whitelist;
// artist/author accept String or List<String> and normalize to array.
inline std::optional<std::string> normalize_chosen(const crow::json::rvalue& ch) {
    if (ch.t() != crow::json::type::Object) return std::nullopt;
    static const char* allowed[] = {"title", "synopsis", "artist", "author", "cover", "tags_add", "tags_hidden"};
    for (const auto& member : ch) {
        const std::string k = std::string(member.key());
        bool ok = false;
        for (const char* a : allowed) if (k == a) { ok = true; break; }
        if (!ok) return std::nullopt;
    }
    crow::json::wvalue out;
    for (const char* key : {"title", "synopsis", "cover"})
        if (ch.has(key) && ch[key].t() == crow::json::type::String) out[key] = ch[key];
    for (const char* key : {"artist", "author"}) {
        if (!ch.has(key)) continue;
        if (ch[key].t() == crow::json::type::String) {
            crow::json::wvalue one(crow::json::type::List);
            one[0] = ch[key];
            out[key] = std::move(one);
        } else if (ch[key].t() == crow::json::type::List) {
            crow::json::wvalue arr(crow::json::type::List);
            int n = 0;
            for (const auto& item : ch[key]) {
                if (item.t() != crow::json::type::String) return std::nullopt;
                arr[n++] = item;
            }
            out[key] = std::move(arr);
        } else return std::nullopt;
    }
    for (const char* key : {"tags_add", "tags_hidden"}) {
        if (!ch.has(key)) continue;
        const auto& arr = ch[key];
        if (arr.t() != crow::json::type::List) return std::nullopt;
        crow::json::wvalue out_arr(crow::json::type::List);
        int n = 0;
        for (const auto& item : arr) {
            if (item.t() != crow::json::type::String) return std::nullopt;
            out_arr[n++] = item;
        }
        out[key] = std::move(out_arr);
    }
    return std::string(out.dump());
}

// GET /api/enrichment/<id>/<detail|covers>?plugin=&slug=&force=0|1
// cached-or-fetch / force per source. Never writes saved_manga / api_cache.
inline crow::response enrichment_source_response(plugin::Host& plugins, BS::thread_pool<>& db,
                                                BS::thread_pool<>& plugin_pool, Store& store,
                                                const std::string& id, const std::string& kind,
                                                const std::string& plugin, const std::string& slug, bool force) {
    if (!valid_title_pair(plugin, slug)) {
        return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
    }
    const std::string title = on_pool(db, [&] {
        const auto row = store.saved_get(plugin, slug);
        return row ? row->name : std::string{};
    });
    if (title.empty()) {
        return crow::response{404, R"({"ok":false,"error":"not in library"})"};
    }

    const auto existing = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!force && existing) {
        std::int64_t fetched_at = existing->fetched_at;
        if (auto cached = stored_source(existing->fetched, id, kind, fetched_at)) {
            crow::response r{*cached};
            r.set_header("Content-Type", "application/json");
            return r;
        }
    }

    const std::string raw = on_pool(plugin_pool, [&] { return plugins.enrich(id, title, kind); });
    glz::generic env;
    if (glz::read_json(env, raw) || !envelope_ok(env)) {
        if (existing) {
            std::int64_t fetched_at = existing->fetched_at;
            if (auto cached = stored_source(existing->fetched, id, kind, fetched_at)) {
                crow::response r{*cached};
                r.set_header("Content-Type", "application/json");
                return r;
            }
        }
        // Sanitize the upstream error: strip control chars and quotes to keep JSON valid.
        std::string err = "enrich failed";
        const auto* o = std::get_if<glz::generic::object_t>(&env.data);
        if (o != nullptr) {
            if (auto it = o->find("error"); it != o->end()) {
                if (const auto* s = std::get_if<std::string>(&it->second.data)) err = *s;
            }
        }
        std::string safe;
        safe.reserve(err.size());
        for (unsigned char ch : err) {
            if (ch >= 32 && ch != '"' && ch != '\\') safe += static_cast<char>(ch);
        }
        if (safe.empty()) safe = "enrich failed";
        crow::response r{502};
        r.set_header("Content-Type", "application/json");
        r.body = std::string(R"({"ok":false,"error":")") + safe + R"("})";
        return r;
    }

    // Plugin succeeded: merge the source's entry into stored `fetched`.
    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    glz::generic fetched;
    if (existing) {
        if (glz::read_json(fetched, existing->fetched)) fetched = glz::generic{};
    }
    if (!fetched.is_array()) {
        fetched = glz::generic{};
        fetched.data = glz::generic::array_t{};
    }
    auto& arr = fetched.get_array();
    for (std::size_t i = 0; i < arr.size();) {
        const auto& obj = arr[i];
        if (!obj.is_object()) { ++i; continue; }
        const auto& o = obj.get_object();
        const auto src = o.find("source");
        const auto* s = src != o.end() ? std::get_if<std::string>(&src->second.data) : nullptr;
        const auto kk = o.find("kind");
        const auto* ks = kk != o.end() ? std::get_if<std::string>(&kk->second.data) : nullptr;
        const std::string entry_kind = ks ? *ks : "detail"; // legacy entries are detail
        if (s != nullptr && *s == id && entry_kind == kind) arr.erase(arr.begin() + i); else ++i;
    }
    glz::generic entry;
    auto& obj = entry.data.emplace<glz::generic::object_t>();
    obj["source"] = glz::generic{};
    obj["source"].data = id;
    obj["kind"] = glz::generic{};
    obj["kind"].data = kind;
    obj["fetched_at"] = glz::generic{};
    obj["fetched_at"].data = static_cast<double>(now);
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data)) {
        if (auto it = o->find("data"); it != o->end()) obj["data"] = it->second;
    }
    arr.push_back(entry);
    const std::string fetched_json = glz::write_json(fetched).value_or("[]");
    on_pool(db, [&] { store.title_meta_put_fetched(plugin, slug, fetched_json, now); return 0; });

    crow::json::wvalue out;
    out["ok"] = true;
    out["cached"] = false;
    out["source"] = id;
    out["fetched_at"] = static_cast<double>(now);
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data)) {
        if (auto it = o->find("data"); it != o->end()) {
            out["data"] = crow::json::load(glz::write_json(it->second).value_or(std::string{}));
        } else {
            out["data"] = crow::json::wvalue();
        }
    } else {
        out["data"] = crow::json::wvalue();
    }
    crow::response r{out};
    r.set_header("Content-Type", "application/json");
    return r;
}

} // namespace api
} // namespace http

namespace http {
namespace api {

inline std::optional<std::string> chosen_cover(Store& store, BS::thread_pool<>& db,
                                               const std::string& plugin, const std::string& slug) {
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!row) return std::nullopt;
    glz::generic chosen;
    if (glz::read_json(chosen, row->chosen) || !chosen.is_object()) return std::nullopt;
    const auto& cd = chosen.get_object();
    const auto it = cd.find("cover");
    if (it == cd.end()) return std::nullopt;
    if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) return *s;
    return std::nullopt;
}

namespace {
inline std::string lower_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
} // namespace

// Merge title_meta.chosen over the scraper detail envelope (design D3):
//   chosen.<field> ?? source; tags = (source tags ∪ chosen.tags_add) − chosen.tags_hidden;
//   cover override is also exposed as `chosen_cover`; `enrichment: {fetched_at, chosen}` rides
//   the top level for the Alpine picker. Corrupt/absent rows return the envelope untouched.
inline std::string apply_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                const std::string& slug, const std::string& envelope) {
    const auto row = on_pool(db, [&] { return store.title_meta_get(plugin, slug); });
    if (!row) return envelope;
    glz::generic env;
    if (glz::read_json(env, envelope) || !envelope_ok(env) || !env.is_object()) return envelope;
    glz::generic chosen;
    if (glz::read_json(chosen, row->chosen) || !chosen.is_object()) return envelope;
    auto& obj = env.get_object();
    const auto& cd = chosen.get_object();

    if (auto data = obj.find("data"); data != obj.end() && data->second.is_object()) {
        auto& d = data->second.get_object();
        const auto set_str = [&](const char* chosen_key, const char* data_key) {
            const auto it = cd.find(chosen_key);
            if (it == cd.end()) return;
            const auto* s = std::get_if<std::string>(&it->second.data);
            if (s != nullptr && !s->empty()) d[data_key] = it->second;
        };
        set_str("title", "name");
        set_str("synopsis", "description");
        set_str("cover", "cover");
        const auto set_names = [&](const char* chosen_key, const char* data_key) {
            const auto it = cd.find(chosen_key);
            if (it == cd.end()) return;
            if (const auto* s = std::get_if<std::string>(&it->second.data)) {
                if (s->empty()) return;
                glz::generic::array_t arr;
                arr.emplace_back().data = *s;
                d[data_key].data = std::move(arr);
            } else if (it->second.is_array()) {
                d[data_key] = it->second;
            }
        };
        set_names("artist", "artists");
        set_names("author", "authors");
        if (const auto it = cd.find("cover"); it != cd.end()) {
            if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr && !s->empty()) {
                d["chosen_cover"] = it->second; // page hydrates from this too
            }
        }

        const auto tadd = cd.find("tags_add");
        const auto thid = cd.find("tags_hidden");
        if (tadd != cd.end() || thid != cd.end()) {
            std::vector<std::string> tags;
            if (const auto it = d.find("tags"); it != d.end() && it->second.is_array()) {
                for (const auto& t : it->second.get_array())
                    if (const auto* s = std::get_if<std::string>(&t.data)) tags.push_back(*s);
            }
            if (tadd != cd.end() && tadd->second.is_array())
                for (const auto& t : tadd->second.get_array())
                    if (const auto* s = std::get_if<std::string>(&t.data)) tags.push_back(*s);
            if (thid != cd.end() && thid->second.is_array()) {
                std::vector<std::string> hidden;
                for (const auto& t : thid->second.get_array())
                    if (const auto* s = std::get_if<std::string>(&t.data)) hidden.push_back(lower_copy(*s));
                std::erase_if(tags, [&](const std::string& t) {
                    for (const auto& h : hidden)
                        if (h == lower_copy(t)) return true;
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

    glz::generic enr;
    auto& e = enr.data.emplace<glz::generic::object_t>();
    e["fetched_at"] = glz::generic{};
    e["fetched_at"].data = static_cast<double>(row->fetched_at);
    e["chosen"] = chosen;
    obj["enrichment"] = std::move(enr);
    return glz::write_json(env).value_or(envelope);
}

} // namespace api
} // namespace http
