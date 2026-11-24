#include "enrich_internal.hpp"

#include "enrich_source_internal.hpp"

namespace http::api {
namespace {

/** Parse an ABI envelope: malformed JSON is a failure, not a thrown error. */
bool parse_ok_envelope(const std::string& body, glz::generic& out) {
    return !glz::read_json(out, body) && envelope_ok(out);
}

/** Last-good envelope already cached for this source+kind, if any. */
std::optional<crow::response> cached_source_response(const std::optional<Store::TitleMetaRow>& existing,
                                                     const std::string& id, const std::string& kind,
                                                     std::int64_t fetched_at) {
    if (auto cached = stored_source(existing ? existing->fetched : std::string{}, id, kind, fetched_at)) {
        crow::response r{*cached};
        r.set_header("Content-Type", "application/json");
        return std::optional<crow::response>{std::move(r)};
    }
    return std::nullopt;
}

/** Upstream error -> valid JSON. Strip control chars/quotes/backslashes so a
 *  hostile error string cannot break the envelope we hand to the browser. */
crow::response enrichment_error_response(const glz::generic& env) {
    std::string err = "enrich failed";
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data); o != nullptr) {
        if (const auto* it = o->find("error"); it != o->end()) {
            if (const auto* s = std::get_if<std::string>(&it->second.data); s != nullptr) {
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

/** Merge the successful source entry into the stored `fetched` catalogue,
 *  replacing the previous entry for the same (source, kind) pair. */
std::string merged_fetched_json(const std::string* old_fetched, const std::string& id, const std::string& kind,
                                const glz::generic& env, std::int64_t now) {
    glz::generic fetched;
    if (old_fetched != nullptr && glz::read_json(fetched, *old_fetched)) {
        fetched = glz::generic{};
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
    obj["source"].data = id;
    obj["kind"].data = kind;
    obj["fetched_at"].data = static_cast<double>(now);
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data); o != nullptr) {
        if (const auto* it = o->find("data"); it != o->end()) {
            obj["data"] = it->second;
        }
    }
    arr.push_back(std::move(entry));
    return glz::write_json(fetched).value_or("[]");
}

/** Normalize the source's alt_titles into a JSON array accepted by
 *  saved_signal_update. Empty string == nothing learned, leave DB untouched. */
std::string alt_titles_json(const glz::generic& env) {
    const auto* o = std::get_if<glz::generic::object_t>(&env.data);
    if (o == nullptr) {
        return {};
    }
    const auto* it = o->find("data");
    if (it == o->end() || !it->second.is_object()) {
        return {};
    }
    const auto& d = it->second.get_object();
    const auto* jt = d.find("alt_titles");
    if (jt == d.end() || !jt->second.is_array()) {
        return {};
    }
    glz::generic::array_t titles;
    for (const auto& t : jt->second.get_array()) {
        // accept plain strings or {title: ...} entries
        if (const auto* s = std::get_if<std::string>(&t.data); s != nullptr) {
            if (!s->empty()) {
                glz::generic v;
                v.data = *s;
                titles.push_back(std::move(v));
            }
        } else if (t.is_object()) {
            if (const auto* tt = t.get_object().find("title"); tt != t.get_object().end()) {
                if (const auto* s = std::get_if<std::string>(&tt->second.data); s != nullptr && !s->empty()) {
                    glz::generic v;
                    v.data = *s;
                    titles.push_back(std::move(v));
                }
            }
        }
    }
    if (titles.empty()) {
        return {};
    }
    glz::generic arr_out;
    arr_out.data = std::move(titles);
    return glz::write_json(arr_out).value_or(std::string{});
}

crow::response enrichment_ok_response(const std::string& id, std::int64_t now, const glz::generic& env) {
    crow::json::wvalue out;
    out["ok"] = true;
    out["cached"] = false;
    out["source"] = id;
    out["fetched_at"] = static_cast<double>(now);
    if (const auto* o = std::get_if<glz::generic::object_t>(&env.data); o != nullptr) {
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

} // namespace

// cached-or-fetch / force per source. Never writes saved_manga / api_cache.
crow::response enrichment_source_response(plugin::Host& plugins, BS::thread_pool<>& db,
                                          BS::thread_pool<>& plugin_pool, Store& store, const std::string& id,
                                          const std::string& kind, const std::string& plugin, const std::string& slug,
                                          bool force) {
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
        // No cached envelope uses code 0; only return it when a row was found.
        if (auto hit = cached_source_response(existing, id, kind, existing->fetched_at)) {
            return std::move(*hit);
        }
    }

    std::string raw = on_pool(plugin_pool, [&] { return plugins.enrich(id, title, kind); });
    glz::generic env;
    if (!parse_ok_envelope(raw, env)) {
        // The library name is often an editorial label the source does not
        // index ("Sengoku Komachi Kurou Tan!" vs MangaDex's "Sengoku Komachi
        // Kuroutan"), which surfaced as "no match found" over a title the
        // source does have. Retry down a short candidate chain (source detail
        // name/title, then the titles already matched by any enrichment
        // source) before falling back to the stored candidate.
        for (const std::string& alt : on_pool(db, [&] { return retry_titles(store, existing, plugin, slug, title); })) {
            const std::string retry = on_pool(plugin_pool, [&] { return plugins.enrich(id, alt, kind); });
            glz::generic env2;
            if (parse_ok_envelope(retry, env2)) {
                raw = retry;
                env = std::move(env2);
                break;
            }
        }
    }
    if (!envelope_ok(env)) {
        if (existing) {
            if (auto hit = cached_source_response(existing, id, kind, existing->fetched_at)) {
                return std::move(*hit);
            }
        }
        return enrichment_error_response(env);
    }

    // Plugin succeeded: merge the source's entry into stored `fetched`.
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    const std::string fetched_json =
        merged_fetched_json(existing ? &existing->fetched : nullptr, id, kind, env, now);
    on_pool(db, [&] {
        store.title_meta_put_fetched(plugin, slug, fetched_json, now);
        return 0;
    });

    // library-dedup 1.2 (enrichment path): feed enrichment alt_titles into
    // saved_manga when a detail envelope lands. Shape: [{title, lang, main}].
    if (kind == "detail") {
        const std::string alts_json = alt_titles_json(env);
        if (!alts_json.empty()) {
            // synopsis left empty -> saved_signal_update leaves it untouched
            on_pool(db, [&, alts_json] {
                store.saved_signal_update(plugin, slug, {}, alts_json);
                return 0;
            });
            // store-contention C1: coalesced recluster via scheduler tick
            store.mark_dup_dirty();
        }
    }

    return enrichment_ok_response(id, now, env);
}

} // namespace http::api
