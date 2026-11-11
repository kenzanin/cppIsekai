#include "enrich_internal.hpp"

namespace http {
namespace api {

void register_enrich(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;

    // GET /api/enrichment — loaded enrichment sources (id, name). Picker needs the list.
    CROW_ROUTE(app, "/api/enrichment")
    ([&] {
        crow::json::wvalue list(crow::json::type::List);
        int i = 0;
        for (const auto& [id, name] : plugins.enrichment_sources()) {
            crow::json::wvalue row;
            row["id"] = id;
            row["name"] = name;
            list[i++] = std::move(row);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["sources"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/api/enrichment/<string>/detail")
    ([&](const crow::request& req, std::string id) -> crow::response {
        const std::string plugin = param(req, "plugin");
        const std::string slug = param(req, "slug");
        const bool force = param(req, "force") == "1";
        return enrichment_source_response(plugins, db, plugin_pool, store, id, "detail", plugin, slug, force);
    });

    CROW_ROUTE(app, "/api/enrichment/<string>/covers")
    ([&](const crow::request& req, std::string id) -> crow::response {
        const std::string plugin = param(req, "plugin");
        const std::string slug = param(req, "slug");
        const bool force = param(req, "force") == "1";
        return enrichment_source_response(plugins, db, plugin_pool, store, id, "covers", plugin, slug, force);
    });

    // POST /api/enrichment/chosen {plugin, slug, chosen: {...}} — full-overwrite under (plugin, slug).
    CROW_ROUTE(app, "/api/enrichment/chosen").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) -> crow::response {
        const auto json = crow::json::load(req.body);
        if (!json || !json.has("plugin") || !json.has("slug") || !json.has("chosen"))
            return crow::response{400, R"({"ok":false,"error":"missing plugin/slug/chosen"})"};
        const std::string plugin = json["plugin"].s();
        const std::string slug = json["slug"].s();
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        crow::json::wvalue chosen_out;
        const auto& ch = json["chosen"];
        if (ch.has("title")) chosen_out["title"] = ch["title"];
        if (ch.has("synopsis")) chosen_out["synopsis"] = ch["synopsis"];
        if (ch.has("artist")) chosen_out["artist"] = ch["artist"];
        if (ch.has("author")) chosen_out["author"] = ch["author"];
        if (ch.has("cover")) chosen_out["cover"] = ch["cover"];
        if (ch.has("tags_add")) chosen_out["tags_add"] = ch["tags_add"];
        if (ch.has("tags_hidden")) chosen_out["tags_hidden"] = ch["tags_hidden"];
        on_pool(db, [&] { store.title_meta_put_chosen(plugin, slug, chosen_out.dump()); return 0; });
        crow::response r{R"({"ok":true})"};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // POST /api/enrichment/reset {plugin, slug, mode} — cover | detail | all; keeps fetched.
    CROW_ROUTE(app, "/api/enrichment/reset").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) -> crow::response {
        const auto json = crow::json::load(req.body);
        if (!json || !json.has("plugin") || !json.has("slug") || !json.has("mode"))
            return crow::response{400, R"({"ok":false,"error":"missing plugin/slug/mode"})"};
        const std::string plugin = json["plugin"].s();
        const std::string slug = json["slug"].s();
        const std::string mode = json["mode"].s();
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        int bad = 0;
        on_pool(db, [&] {
            if (mode == "cover") store.title_meta_reset_cover(plugin, slug);
            else if (mode == "detail") store.title_meta_reset_detail(plugin, slug);
            else if (mode == "all") { store.title_meta_reset_cover(plugin, slug); store.title_meta_reset_detail(plugin, slug); }
            else bad = 1;
            return 0;
        });
        if (bad) return crow::response{400, R"({"ok":false,"error":"mode must be cover|detail|all"})"};
        crow::response r{R"({"ok":true})"};
        r.set_header("Content-Type", "application/json");
        return r;
    });
}

} // namespace api
} // namespace http
