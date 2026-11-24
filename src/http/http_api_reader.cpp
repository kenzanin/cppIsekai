#include "api_ctx.hpp"

#include <algorithm>
#include <optional>

namespace http::api {


// Reader position/window routes (split from the http_api monolith; the
// position write path, the continue-hint and the skip-rerun anchor).
void register_reader(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& prefetch = c.prefetch;

    // reader-page 2.2: continue-hint data. db worker (store.hpp ponytail);
    // absent position = 404 so the detail badge stays hidden.
    CROW_ROUTE(app, "/api/position/<string>/<string>")
    ([&](const crow::request&, const std::string& plugin, const std::string& manga) -> crow::response {
        const auto pos = on_pool(db, [&] { return store.position_get(plugin, manga); });
        if (!pos) {
            crow::response r{R"({"ok":false})"};
            r.code = 404;
            r.set_header("Content-Type", "application/json");
            return r;
        }
        crow::json::wvalue out;
        out["ch"] = pos->chapter_number;
        out["page"] = pos->page;
        out["mode"] = pos->mode;
        out["direction"] = pos->direction;
        out["zoom"] = pos->zoom;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Reader prefetch input (image-cache): remember the position -- it is the
    // anchor the skip-rerun reads back -- and report the window. No worker
    // here; task 3.3 consumes the returned page list.
    CROW_ROUTE(app, "/reader/position")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            std::string chapter;
            std::string manga;
            std::string plugin;
            if (body) {
                chapter = str_field(body, "chapter");
                manga = str_field(body, "manga");
                plugin = str_field(body, "plugin");
            }
            if (chapter.empty() || manga.empty() || plugin.empty()) {
                crow::response r{R"({"ok":false,"error":"chapter, manga and plugin are required"})"};
                r.code = 400;
                r.set_header("Content-Type", "application/json");
                return r;
            }
            const reader::Position pos{.chapter_id = chapter,
                                       .page = static_cast<int>(std::max(1L, num_field(body, "page", 1)))};

            // reader-page 2.2: optional mode/direction -- absent keeps the stored
            // value (the same db hop stamps updated_at and stores the result).
            std::string mode;
            std::string direction;
            std::string zoom;
            if (body) {
                mode = str_field(body, "mode");
                direction = str_field(body, "direction");
                zoom = str_field(body, "zoom");
            }
            // The chapter NUMBER rides along (the reader knows it from ?ch/the
            // pages list); absent keeps the stored one. Feeds the resume hint
            // and the prefetch worker's v3 placement.
            // ch 0 is valid; absence is signaled by the optional, not by 0.
            std::optional<double> chapter_number;
            if (body && body.has("chapter_number") && body["chapter_number"].t() == crow::json::type::Number) {
                chapter_number = body["chapter_number"].d();
            }
            on_pool(db, [&] {
                const auto stored = store.position_get(plugin, manga);
                ReaderPosition next{
                    .plugin = plugin,
                    .slug = manga,
                    .chapter_id = chapter,
                    .chapter_number = resolve_chapter_number(stored, chapter_number),
                    .page = pos.page,
                    .mode = mode,
                    .direction = direction,
                    .zoom = zoom,
                    .updated_at = 0,
                };
                if (stored) {
                    // same chapter, no page sent -> keep the page
                    if (chapter == stored->chapter_id) {
                        next.page = pos.page > 1 ? pos.page : stored->page;
                    }
                }
                store.position_set(merge_position(stored, next));
                // chapter-list-controls: max pages-read lives on the master row
                // (chapters.read_max); the read bit flips host-side at the stored
                // page count (chapter-store 3.3/D5) -- last page == read, no
                // client flagging. count==0 = pages never parsed; no threshold.
                if (!chapter.empty()) {
                    const int p = pos.page > 1 ? pos.page : 1;
                    store.chapter_read_max_bump(plugin, manga, chapter, p);
                }
                // library-grid-stats 2.4: reader touch stamps last_read_at + clears the New bit.
                store.saved_clear_update(plugin, manga);
                spdlog::debug("position {} {} ch={} p={} cn={} mode='{}' dir='{}'", plugin, manga, chapter, next.page,
                              next.chapter_number, next.mode, next.direction);
                return 0;
            });

            const std::vector<reader::WindowPart> found = reader_window(store, db, plugin_pool, plugins, plugin, manga, pos);
            std::vector<crow::json::wvalue> window;
            for (const auto& part : found) {
                for (const auto& url : part.urls) {
                    window.emplace_back(url);
                }
            }
            // Parts carry their own chapter id/number/first-page: the worker
            // lands each slice in that chapter's dir, spill included.
            if (!found.empty()) {
                std::size_t n = 0;
                for (const auto& p : found) {
                    n += p.urls.size();
                }
                spdlog::debug("nudge {} urls in {} part(s), {}.{}", n, found.size(), plugin, manga);
                prefetch.nudge(found, plugin, manga);
            }

            crow::json::wvalue out;
            out["window"] = std::move(window);
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });
}

} // namespace http::api
