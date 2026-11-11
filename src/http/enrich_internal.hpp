#pragma once

/** \file enrich_internal.hpp
 *  \brief Helpers for the enrichment HTTP routes (http_api_enrich.cpp); inline
 *  header so the route TU stays small. Not public API. */

#include "api_ctx.hpp"

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
        if (auto cached = stored_source(existing->fetched, id, fetched_at)) {
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
            if (auto cached = stored_source(existing->fetched, id, fetched_at)) {
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
        const auto src = obj.get_object().find("source");
        const auto* s = src != obj.get_object().end() ? std::get_if<std::string>(&src->second.data) : nullptr;
        if (s != nullptr && *s == id) arr.erase(arr.begin() + i); else ++i;
    }
    glz::generic entry;
    auto& obj = entry.data.emplace<glz::generic::object_t>();
    obj["source"] = glz::generic{};
    obj["source"].data = id;
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
