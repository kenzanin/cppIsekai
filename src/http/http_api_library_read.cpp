#include "api_ctx.hpp"
#include "enrich_internal.hpp"

namespace http::api {

void register_library_read(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;

    CROW_ROUTE(app, "/api/library")
    ([&] {
        const auto rows = on_pool(db, [&] { return store.saved_list(); });
        // One sqlite_orm IN query per plugin replaces two title_meta_get calls
        // per row; parse `chosen` once and read both fields from that object.
        std::vector<std::pair<std::string, std::string>> tm_keys;
        tm_keys.reserve(rows.size());
        for (const auto& r : rows) {
            tm_keys.emplace_back(r.plugin, r.slug);
        }
        const auto tm = on_pool(db, [&] { return store.title_meta_get_many(tm_keys); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& r : rows) {
            crow::json::wvalue row;
            row["plugin"] = r.plugin;
            row["slug"] = r.slug;
            glz::generic chosen;
            const auto it = tm.find({r.plugin, r.slug});
            const bool have = it != tm.end() && !glz::read_json(chosen, it->second.chosen) && chosen.is_object();
            row["name"] = (have ? chosen_field(chosen, "title") : std::nullopt).value_or(r.name);
            row["cover"] = (have ? chosen_field(chosen, "cover") : std::nullopt).value_or(r.cover);
            row["added_at"] = static_cast<double>(r.added_at);
            row["status"] = r.status;
            row["total_ch"] = r.total_ch;
            row["read_ch"] = r.read_ch;
            row["status_flags"] = r.status_flags; // bit0 carries the New state (was has_update)
            row["pinned"] = r.pinned; // library-pin: grid Pinned section membership
            // library-ownership 3.2: derived upstream state drives the orphan
            // badge/filter; never stored, recomputed per request.
            const auto up = store.plugin_upstream(r.plugin);
            row["state"] = up == Upstream::orphan ? "orphan"
                            : (up == Upstream::offline ? "offline" : "live");
            row["last_read_at"] = static_cast<double>(r.last_read_at);
            row["updated_at"] = static_cast<double>(r.updated_at);
            // manga-detail-table 1.3: expose the materialized display columns (raw
            // JSON arrays stored verbatim); absent until first materialization.
            const auto put_arr = [&row](const char* key, const std::string& text) {
                if (text.empty()) {
                    return;
                }
                if (const auto parsed = crow::json::load(text)) {
                    row[key] = crow::json::wvalue(parsed);
                }
            };
            put_arr("authors", r.authors);
            put_arr("artists", r.artists);
            put_arr("tags", r.tags);
            if (!r.synopsis_display.empty()) {
                row["synopsis_display"] = r.synopsis_display;
            }
            list[idx++] = std::move(row);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["data"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/library").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        const std::string body = req.body;
        const auto json = crow::json::load(body);
        if (!json || !json.has("plugin") || !json.has("slug")) {
            return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
        }
        SavedManga m;
        m.plugin = json["plugin"].s();
        m.slug = json["slug"].s();
        m.name = json.has("name") && json["name"].t() == crow::json::type::String ? json["name"].s()
                                                                                  : std::string{json["slug"].s()};
        m.cover =
            json.has("cover") && json["cover"].t() == crow::json::type::String ? json["cover"].s() : std::string{};
        on_pool(db, [&] {
            // manga-detail-table D9: the detail page submits detail.name || slug. A
            // pre-materialization row opened with the plugin down would overwrite a
            // real stored name with the slug/empty -- keep the stored one instead.
            if (m.name.empty() || m.name == m.slug) {
                if (const auto existing = store.saved_get(m.plugin, m.slug);
                    existing.has_value() && !existing->name.empty() && existing->name != m.slug) {
                    m.name = existing->name;
                }
            }
            store.save_manga(m);
            store.recompute_dup_groups();
            return 0;
        }); // dedup 2.3
        // library-promote-on-save 1.1: promote whatever the preceding open
        // cached -- same materializers the detail route runs, zero plugin
        // calls; silent no-op when nothing is cached (1.2).
        http::api::promote_saved_cached(store, db, m.plugin, m.slug);
        crow::response r{R"({"ok":true,"saved":true})"};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/library/pin")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto json = crow::json::load(req.body);
            if (!json || !json.has("plugin") || !json.has("slug")) {
                return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
            }
            const bool pinned = !json.has("pinned") || json["pinned"].b();
            on_pool(db, [&] {
                store.saved_set_pinned(json["plugin"].s(), json["slug"].s(), pinned);
                return 0;
            });
            crow::response r{pinned ? R"({"ok":true,"pinned":true})" : R"({"ok":true,"pinned":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });

}

} // namespace http::api
