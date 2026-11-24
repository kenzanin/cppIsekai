#include "enrich_internal.hpp"

#include "enrich_source_internal.hpp"

namespace http::api {


// cached-or-fetch / force per source. Never writes saved_manga / api_cache.
 crow::response enrichment_source_response(plugin::Host& plugins, BS::thread_pool<>& db,
                                                 BS::thread_pool<>& plugin_pool, Store& store, const std::string& id,
                                                 const std::string& kind, const std::string& plugin,
                                                 const std::string& slug, bool force) {
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

    const auto ok_env = [](const std::string& body, glz::generic& out) {
        return !glz::read_json(out, body) && envelope_ok(out);
    };
    std::string raw = on_pool(plugin_pool, [&] { return plugins.enrich(id, title, kind); });
    glz::generic env;
    if (!ok_env(raw, env)) {
        // The library name is often an editorial label the source does not
        // index ("Sengoku Komachi Kurou Tan!" vs MangaDex's "Sengoku Komachi
        // Kuroutan"), which surfaced as "no match found" over a title the
        // source does have. Retry down a short candidate chain (source detail
        // name/title, then the titles already matched by any enrichment
        // source) before falling back to the stored candidate.
        for (const std::string& alt : on_pool(db, [&] { return retry_titles(store, plugin, slug, title); })) {
            const std::string retry = on_pool(plugin_pool, [&] { return plugins.enrich(id, alt, kind); });
            glz::generic env2;
            if (ok_env(retry, env2)) {
                raw = retry;
                env = std::move(env2);
                break;
            }
        }
    }
    if (!envelope_ok(env)) {
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
            if (const auto* it = o->find("error"); it != o->end()) {
                if (const auto* s = std::get_if<std::string>(&it->second.data)) {
                    err = *s;
                }
            }
        }
        std::string safe;
        safe.reserve(err.size());
        for (unsigned char ch : err) {
            if (ch >= 32 && ch != '"' && ch != '\\') {
                safe += static_cast<char>(ch);
            }
        }
        if (safe.empty()) {
            safe = "enrich failed";
        }
        crow::response r{502};
        r.set_header("Content-Type", "application/json");
        r.body = std::string(R"({"ok":false,"error":")") + safe + R"("})";
        return r;
    }

    // Plugin succeeded: merge the source's entry into stored `fetched`.
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    glz::generic fetched;
    if (existing) {
        if (glz::read_json(fetched, existing->fetched)) {
            fetched = glz::generic{};
        }
    }
    if (!fetched.is_array()) {
        fetched = glz::generic{};
        fetched.data = glz::generic::array_t{};
    }
    auto& arr = fetched.get_array();
    for (std::size_t i = 0; i < arr.size();) {
        const auto& obj = arr[i];
        if (!obj.is_object()) {
            ++i;
            continue;
        }
        const auto& o = obj.get_object();
        const auto* const src = o.find("source");
        const auto* s = src != o.end() ? std::get_if<std::string>(&src->second.data) : nullptr;
        const auto* const kk = o.find("kind");
        const auto* ks = kk != o.end() ? std::get_if<std::string>(&kk->second.data) : nullptr;
        const std::string entry_kind = (ks != nullptr) ? *ks : "detail"; // legacy entries are detail
        if (s != nullptr && *s == id && entry_kind == kind) {
            arr.erase(arr.begin() + i);
        } else {
            ++i;
        }
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
        if (const auto* it = o->find("data"); it != o->end()) {
            obj["data"] = it->second;
        }
    }
    arr.push_back(entry);
    const std::string fetched_json = glz::write_json(fetched).value_or("[]");
    on_pool(db, [&] {
        store.title_meta_put_fetched(plugin, slug, fetched_json, now);
        return 0;
    });
    // library-dedup 1.2 (enrichment path): feed enrichment alt_titles into
    // saved_manga when a detail envelope lands. Shape: [{title, lang, main}].
    if (kind == "detail") {
        if (const auto* o = std::get_if<glz::generic::object_t>(&env.data)) {
            if (const auto* it = o->find("data"); it != o->end() && it->second.is_object()) {
                const auto& d = it->second.get_object();
                if (const auto* jt = d.find("alt_titles"); jt != d.end() && jt->second.is_array()) {
                    glz::generic::array_t titles;
                    for (const auto& t : jt->second.get_array()) {
                        // accept plain strings or {title: ...} entries
                        if (const auto* s = std::get_if<std::string>(&t.data)) {
                            if (!s->empty()) {
                                glz::generic v;
                                v.data = *s;
                                titles.push_back(std::move(v));
                            }
                        } else if (t.is_object()) {
                            if (const auto* tt = t.get_object().find("title"); tt != t.get_object().end()) {
                                if (const auto* s = std::get_if<std::string>(&tt->second.data);
                                    s != nullptr && !s->empty()) {
                                    glz::generic v;
                                    v.data = *s;
                                    titles.push_back(std::move(v));
                                }
                            }
                        }
                    }
                    if (!titles.empty()) {
                        glz::generic arr_out;
                        arr_out.data = std::move(titles);
                        const std::string alts_json = glz::write_json(arr_out).value_or(std::string{});
                        // synopsis left empty → saved_signal_update leaves it untouched
                        on_pool(db, [&, alts_json] {
                            store.saved_signal_update(plugin, slug, {}, alts_json);
                            return 0;
                        });
                        // store-contention C1: coalesced recluster via scheduler tick
                        store.mark_dup_dirty();
                    }
                }
            }
        }
    }

    crow::json::wvalue out;
    out["ok"] = true;
    out["cached"] = false;
    out["source"] = id;
    out["fetched_at"] = static_cast<double>(now);
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data)) {
        if (const auto* it = o->find("data"); it != o->end()) {
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

} // namespace http::api
