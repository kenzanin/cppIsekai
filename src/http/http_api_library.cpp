#include "api_ctx.hpp"
#include "enrich_internal.hpp"

namespace http::api {

// library-save 2.1: saved-manga routes (split from the http_api monolith;
// bodies verbatim). POST-only writes (codebase convention); GET is the grid
// source and the detail-page toggle membership.
void register_library(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& images = c.images;

    CROW_ROUTE(app, "/api/library")
    ([&] {
        const auto rows = on_pool(db, [&] { return store.saved_list(); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& r : rows) {
            crow::json::wvalue row;
            row["plugin"] = r.plugin;
            row["slug"] = r.slug;
            const auto ot = chosen_title(store, db, r.plugin, r.slug);
            row["name"] = ot.value_or(r.name);
            const auto ov = chosen_cover(store, db, r.plugin, r.slug);
            row["cover"] = ov.value_or(r.cover);
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

    CROW_ROUTE(app, "/api/library/remove")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto json = crow::json::load(req.body);
            if (!json || !json.has("plugin") || !json.has("slug")) {
                return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
            }
            const std::string plugin = json["plugin"].s();
            const std::string slug = json["slug"].s();
            // library-promote-on-save 4.2: unsave wipes the pair -- satellites +
            // envelope cache in the store op, then the disk image tree. The
            // cover URL must be captured BEFORE the row goes.
            const std::string cover = on_pool(db, [&] {
                std::string cov;
                if (const auto row = store.saved_get(plugin, slug)) {
                    cov = row->cover;
                }
                store.unsave_manga(plugin, slug); // full purge, one txn + api_cache
                store.recompute_dup_groups();
                return cov;
            });
            on_pool(plugin_pool, [&] {
                images.purge_pair(plugin, slug, {cover}); // page tree + cover files
                return 0;
            }); // dedup 2.3
            crow::response r{R"({"ok":true,"saved":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // library-ownership 3.5: mass cleanup for retained entries whose plugin is
    // gone. Same wipe path as the single remove (rows + api_cache + images);
    // live/offline entries are untouched.
    CROW_ROUTE(app, "/api/library/purge-orphans")
        .methods(crow::HTTPMethod::POST)([&]() -> crow::response {
            const auto orphans = on_pool(db, [&] { return store.orphan_list(); });
            int purged = 0;
            for (const auto& [p, s] : orphans) {
                const std::string cover = on_pool(db, [&] {
                    std::string c;
                    if (const auto row = store.saved_get(p, s)) {
                        c = row->cover;
                    }
                    store.unsave_manga(p, s);
                    store.recompute_dup_groups();
                    return c;
                });
                on_pool(plugin_pool, [&] {
                    images.purge_pair(p, s, {cover});
                    return 0;
                });
                ++purged;
            }
            crow::json::wvalue out;
            out["ok"] = true;
            out["purged"] = purged;
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // library-migrate 2.1: repoint the saved entry at another source. The
    // target's detail+chapters envelopes are fetched here (cached path, same
    // as a plain open) so the store txn is pure DB; chapters rows materialize
    // first, then migrate_source rekeys + carries the read boundary.
    CROW_ROUTE(app, "/api/migrate")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto json = crow::json::load(req.body);
            const auto field = [&](const char* k) {
                return json && json.has(k) && json[k].t() == crow::json::type::String ? std::string{json[k].s()}
                                                                                      : std::string{};
            };
            const auto bad = [](std::string msg) {
                crow::json::wvalue out;
                out["ok"] = false;
                out["error"] = std::move(msg);
                crow::response r{out};
                r.set_header("Content-Type", "application/json");
                return r;
            };
            const std::string from_plugin = field("from_plugin");
            const std::string from_slug = field("from_slug");
            const std::string to_plugin = field("to_plugin");
            const std::string to_slug = field("to_slug");
            if (from_plugin.empty() || from_slug.empty() || to_plugin.empty() || to_slug.empty()) {
                return bad("missing from/to plugin/slug");
            }
            if (!valid_title_pair(to_plugin, to_slug)) {
                return bad("bad target");
            }
            // pre-flight on the db pool: clean errors before any plugin call
            if (!on_pool(db, [&] { return store.saved_get(from_plugin, from_slug); })) {
                return bad("source not in library");
            }
            if (on_pool(db, [&] { return store.saved_get(to_plugin, to_slug); })) {
                return bad("target already in library");
            }
            if (!on_pool(db, [&] { return store.plugin_enabled(to_plugin); })) {
                return bad("target plugin disabled");
            }

            crow::json::wvalue p;
            p["id"] = to_slug;
            const std::string params = p.dump();
            const auto fetch_env = [&](const char* method) {
                return on_pool(plugin_pool, [&] {
                    return call_cached(store, to_plugin, method, params,
                                       [&] { return plugins.call(to_plugin, method, params); }, &plugins);
                });
            };
            const std::string detail_env = fetch_env("detail");
            const std::string chapters_env = fetch_env("chapters");
            const auto chs = envelope_data(chapters_env);
            if (!chs) {
                return bad("target chapters unavailable");
            }
            std::string name;
            std::string cover;
            std::string status;
            if (const auto obj = ::detail::envelope_object(detail_env)) {
                name = ::detail::envelope_str(*obj, "name");
                cover = ::detail::envelope_str(*obj, "cover");
                status = ::detail::envelope_str(*obj, "status");
            }
            // chapters identity rows first (legacy skip/read inheritance and
            // the fingerprint ride inside chapters_upsert)
            materialize_chapters(store, db, to_plugin, to_slug, chapters_env);
            const int total = static_cast<int>(chs->size());
            const bool ok = on_pool(db, [&] {
                const bool moved =
                    store.migrate_source(from_plugin, from_slug, to_plugin, to_slug,
                                         {.name = name, .cover = cover, .status = status, .total_ch = total});
                if (moved) {
                    store.recompute_dup_groups();
                }
                return moved;
            });
            if (!ok) {
                return bad("migrate failed (raced library change?)");
            }
            crow::json::wvalue out;
            out["ok"] = true;
            out["plugin"] = to_plugin;
            out["slug"] = to_slug;
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });
}

} // namespace http::api
