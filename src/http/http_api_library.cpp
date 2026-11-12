#include "api_ctx.hpp"
#include "enrich_internal.hpp"

namespace http {
namespace api {

// library-save 2.1: saved-manga routes (split from the http_api monolith;
// bodies verbatim). POST-only writes (codebase convention); GET is the grid
// source and the detail-page toggle membership.
void register_library(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;

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
            row["has_update"] = r.has_update;
            row["last_read_at"] = static_cast<double>(r.last_read_at);
            row["updated_at"] = static_cast<double>(r.updated_at);
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
        if (!json || !json.has("plugin") || !json.has("slug"))
            return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
        SavedManga m;
        m.plugin = json["plugin"].s();
        m.slug = json["slug"].s();
        m.name = json.has("name") && json["name"].t() == crow::json::type::String ? json["name"].s()
                                                                                  : std::string{json["slug"].s()};
        m.cover =
            json.has("cover") && json["cover"].t() == crow::json::type::String ? json["cover"].s() : std::string{};
        on_pool(db, [&] {
            store.save_manga(m);
            store.recompute_dup_groups();
            return 0;
        }); // dedup 2.3
        crow::response r{R"({"ok":true,"saved":true})"};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/library/remove")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto json = crow::json::load(req.body);
            if (!json || !json.has("plugin") || !json.has("slug"))
                return crow::response{R"({"ok":false,"error":"missing plugin/slug"})"};
            const std::string plugin = json["plugin"].s();
            const std::string slug = json["slug"].s();
            on_pool(db, [&] {
                store.unsave_manga(plugin, slug);
                store.recompute_dup_groups();
                return 0;
            }); // dedup 2.3
            crow::response r{R"({"ok":true,"saved":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });
}

} // namespace api
} // namespace http
