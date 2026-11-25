#include "enrich/enrich_internal.hpp"
#include "http_routes.hpp"

#include "assets/assets.hpp"
#include "plugin/url_codec.hpp"

#include <glaze/glaze.hpp>
#include <optional>
#include <utility>

#include <fstream>
#include <iterator>
#include <sstream>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

#include "http_pages_context.hpp"

namespace http {

void register_pages_library(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, LogBuffer& /*logs*/,
                    reader::CoverWorker& covers) {
    CROW_ROUTE(app, "/")
    ([&](const crow::request& req) { return html("home.mustache", home_context(store, db, req, covers)); });

    // htmx swaps only the grid; the sidebar and top bar stay put.
    CROW_ROUTE(app, "/library/grid")
    ([&](const crow::request& req) { return html("grid.mustache", home_context(store, db, req, covers)); });

    CROW_ROUTE(app, "/search")([&] { return html("search.mustache", layout(store, db, "search")); });

    // reader (reader-page 3.2): the reader page at /read. Server-side shell
    // only (D1): pages load from /api/pages. The store seed is resume-only
    // (db worker, ponytail in store.hpp); ?ch/?page in the URL override it
    // client-side in boot(), and empty mode/direction leave the JS defaults
    // (paged rtl) untouched.
    CROW_ROUTE(app, "/read/<string>/<string>")
    ([&](const crow::request&, const std::string& plugin, const std::string& slug) {
        const auto pos = on_pool(db, [&] { return store.position_get(plugin, slug); });
        int ch = 1;
        int page = 1;
        std::string mode;
        std::string dir;
        std::string zoom;
        if (pos) {
            ch = std::max(0, static_cast<int>(pos->chapter_number)); // ch 0 is valid
            page = std::max(1, pos->page);
            mode = pos->mode;
            dir = pos->direction;
            zoom = pos->zoom;
        }
        // Reader chrome shows the same display title as the detail page:
        // chosen override > saved library name > site slug. One pool round-trip;
        // chosen_field parses the row already fetched (no nested on_pool).
        const std::string reader_title = on_pool(db, [&] {
            std::string t;
            if (const auto tm = store.title_meta_get(plugin, slug)) {
                glz::generic chosen;
                if (!glz::read_json(chosen, tm->chosen) && chosen.is_object()) {
                    if (const auto v = http::api::chosen_field(chosen, "title")) {
                        t = *v;
                    }
                }
            }
            if (t.empty()) {
                if (const auto row = store.saved_get(plugin, slug)) {
                    t = row->name;
                }
            }
            return t.empty() ? slug : t;
        });
        auto ctx = layout(store, db, "library");
        ctx["reader_title"] = reader_title;
        ctx["resume_ch"] = ch;
        ctx["resume_page"] = page;
        ctx["resume_mode"] = mode;
        ctx["resume_dir"] = dir;
        ctx["resume_zoom"] = zoom;
        return html("reader.mustache", ctx);
    });

    // manga-detail: the shelf page (metadata + chapter list, hydrating from
    // /api/*). Reader is at /read.
    CROW_ROUTE(app, "/view/manga/<string>/<string>")
    ([&](const crow::request&, std::string plugin, const std::string&) {
        auto ctx = layout(store, db, "library");
        // 0 = disabled: the page's manual-update button renders dead (plugin-lifecycle 1.3)
        ctx["pe"] = on_pool(db, [&] { return store.plugin_enabled(plugin); }) ? 1 : 0;
        return html("detail.mustache", ctx);
    });

    // library-migrate 1.1: source picker shell; results hydrate client-side
    // exactly like the search page (per-plugin /plugin/<id>/search), then
    // POST /api/migrate rekeys the row server-side.
    CROW_ROUTE(app, "/view/migrate/<string>/<string>")
    ([&](const crow::request&, std::string plugin, std::string slug) -> crow::response {
        const auto row = on_pool(db, [&] { return store.saved_get(plugin, slug); });
        if (!row) { // unsaved/deleted pair: back to the detail page it came from
            crow::response r;
            r.code = 302;
            r.set_header("Location", "/view/manga/" + plugin + "/" + slug);
            return r;
        }
        auto ctx = layout(store, db, "library");
        ctx["plugin"] = plugin;
        ctx["slug"] = slug;
        ctx["name"] = row->name;
        ctx["src_ch"] = row->total_ch;
        ctx["src_read"] = row->read_ch;
        // exact-badge corpus: normalized name + alt titles, comma-joined (the
        // keys are [a-z0-9]+ so the separator cannot collide)
        std::string keys = title_key(row->name);
        glz::generic arr;
        if (!row->alt_titles.empty() && glz::read_json(arr, row->alt_titles) == glz::error_code{} && arr.is_array()) {
            for (const auto& el : arr.get_array()) {
                if (const auto* s = std::get_if<std::string>(&el.data); s != nullptr && !s->empty()) {
                    const std::string k = title_key(*s);
                    if (!k.empty()) {
                        keys += ',';
                        keys += k;
                    }
                }
            }
        }
        ctx["altkeys"] = keys;
        return html("migrate.mustache", ctx);
    });

}

} // namespace http
