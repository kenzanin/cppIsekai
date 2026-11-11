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
        const auto& ch = json["chosen"];
        if (ch.t() != crow::json::type::Object)
            return crow::response{400, R"({"ok":false,"error":"chosen must be an object"})"};
        // exact key whitelist: unknown keys reject the whole write (task 3.2)
        for (const auto& member : ch) { // crow rvalue: range-for yields rvalue&, keys come from .key()
            const std::string k = std::string(member.key());
            static const char* allowed[] = {"title", "synopsis", "artist", "author", "cover", "tags_add", "tags_hidden"};
            bool ok_key = false;
            for (const char* a : allowed) if (k == a) { ok_key = true; break; }
            if (!ok_key)
                return crow::response{400, R"({"ok":false,"error":"unknown chosen key"})"};
        }
        crow::json::wvalue chosen_out;
        for (const char* key : {"title", "synopsis", "cover"})
            if (ch.has(key) && ch[key].t() == crow::json::type::String) chosen_out[key] = ch[key];
        for (const char* key : {"artist", "author"}) {
            // String (single name) or List of strings; normalized to array so apply_chosen's union path works.
            if (!ch.has(key)) continue;
            if (ch[key].t() == crow::json::type::String) {
                crow::json::wvalue one(crow::json::type::List);
                one[0] = ch[key];
                chosen_out[key] = std::move(one);
            } else if (ch[key].t() == crow::json::type::List) {
                crow::json::wvalue out_arr(crow::json::type::List);
                int n = 0;
                for (const auto& item : ch[key]) {
                    if (item.t() != crow::json::type::String)
                        return crow::response{400, R"({"ok":false,"error":"artist/author array must be strings"})"};
                    out_arr[n++] = item;
                }
                chosen_out[key] = std::move(out_arr);
            } else {
                return crow::response{400, R"({"ok":false,"error":"artist/author must be a string or array of strings"})"};
            }
        }
        for (const char* key : {"tags_add", "tags_hidden"}) {
            if (!ch.has(key)) continue;
            const auto& arr = ch[key];
            if (arr.t() != crow::json::type::List)
                return crow::response{400, R"({"ok":false,"error":"tags_add/tags_hidden must be an array of strings"})"};
            crow::json::wvalue out_arr(crow::json::type::List);
            int n = 0;
            for (const auto& item : arr) {
                if (item.t() != crow::json::type::String)
                    return crow::response{400, R"({"ok":false,"error":"tags_add/tags_hidden must be an array of strings"})"};
                out_arr[n++] = item;
            }
            chosen_out[key] = std::move(out_arr);
        }
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
