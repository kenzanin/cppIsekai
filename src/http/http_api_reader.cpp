#include "api_ctx.hpp"

#include <algorithm>
#include <optional>

namespace http {
namespace api {

// The reader window composition (image-cache): chapter list + pages through
// the cached ABI path (stale-serve included), skip flags from plugin_state,
// math in reader::next_window. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
// Declared in api_ctx.hpp; the state TU reuses it for the skip-rerun.
std::vector<std::string> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                       plugin::Host& plugins, const std::string& plugin_id,
                                       const std::string& manga_id, const reader::Position& pos) {
    crow::json::wvalue query;
    query["id"] = manga_id;
    const std::string chapters_query = query.dump();

    const std::string envelope = on_pool(plugin_pool, [&] {
        return call_cached(store, plugin_id, "chapters", chapters_query,
                           [&] { return plugins.call(plugin_id, "chapters", chapters_query); });
    });

    std::vector<reader::Chapter> chapters;
    if (const auto data = envelope_data(envelope)) {
        for (const auto& item : *data) {
            if (!item.is_object())
                continue;
            const auto& entry = item.get_object();
            const auto id = entry.find("id");
            const auto* id_s = id == entry.end() ? nullptr : std::get_if<std::string>(&id->second.data);
            if (!id_s)
                continue;
            double number = 0; // fractional chapter numbers are a thing
            if (const auto num = entry.find("number"); num != entry.end())
                if (const auto* d = std::get_if<double>(&num->second.data))
                    number = *d;
            chapters.push_back(reader::Chapter{*id_s, number});
        }
    }
    if (chapters.empty()) {
        spdlog::warn("reader: no chapters for plugin='{}' manga='{}'", plugin_id, manga_id);
        return {};
    }

    reader::Skipped skip;
    on_pool(db, [&] {
        for (const auto& ch : chapters)
            if (const std::optional<std::string> v = store.state_get(kStateHost, std::string(kSkipPrefix) + ch.id))
                skip[ch.id] = *v == "1";
        return 0; // on_pool is value-based
    });

    const auto pages_of = [&](const std::string& chapter_id) {
        crow::json::wvalue q;
        q["id"] = chapter_id;
        const std::string pages_query = q.dump();
        const std::string env = on_pool(plugin_pool, [&] {
            return call_cached(store, plugin_id, "pages", pages_query,
                               [&] { return plugins.call(plugin_id, "pages", pages_query); });
        });
        std::vector<std::string> urls;
        if (const auto data = envelope_data(env))
            for (const auto& el : *data)
                if (const auto* s = std::get_if<std::string>(&el.data))
                    urls.emplace_back(*s);
        return urls;
    };

    const std::vector<std::string> window = reader::next_window(chapters, pages_of, pos, skip);
    spdlog::info("reader window {} p{} -> {} page(s)", pos.chapter_id, pos.page, window.size());
    return window;
}

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
            std::string chapter, manga, plugin;
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
            const reader::Position pos{chapter, static_cast<int>(std::max(1L, num_field(body, "page", 1)))};

            // reader-page 2.2: optional mode/direction -- absent keeps the stored
            // value (the same db hop stamps updated_at and stores the result).
            std::string mode, direction, zoom;
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
            if (body && body.has("chapter_number") && body["chapter_number"].t() == crow::json::type::Number)
                chapter_number = body["chapter_number"].d();
            on_pool(db, [&] {
                const auto stored = store.position_get(plugin, manga);
                ReaderPosition next{plugin, manga, chapter, resolve_chapter_number(stored, chapter_number), pos.page, mode, direction, zoom, 0};
                if (stored) {
                    // same chapter, no page sent -> keep the page
                    if (chapter == stored->chapter_id)
                        next.page = pos.page > 1 ? pos.page : stored->page;
                }
                store.position_set(merge_position(stored, next));
                // detail-enrichment 1.1 (chapter-list-controls): max pages-read per chapter for the row counter/check.
                if (!chapter.empty()) {
                    int prev = 0;
                    if (const auto s = store.state_get(kStateHost, std::string(kReadPrefix) + chapter); s && !s->empty())
                        prev = std::atoi(s->c_str()); // state_get returns the stored "N"
                    const int p = pos.page > 1 ? pos.page : 1;
                    const int now = std::max(prev, p);
                    store.state_set(kStateHost, std::string(kReadPrefix) + chapter, std::to_string(now));
                }
                spdlog::debug("position {} {} ch={} p={} cn={} mode='{}' dir='{}'", plugin, manga, chapter,
                              next.page, next.chapter_number, next.mode, next.direction);
                return 0;
            });

            const std::vector<std::string> found =
                reader_window(store, db, plugin_pool, plugins, plugin, manga, pos);
            std::vector<crow::json::wvalue> window;
            for (const auto& url : found)
                window.emplace_back(url);
            if (!found.empty()) {
                if (chapter_number) { // v3 placement; else the sha fallback
                    spdlog::debug("nudge {} urls, ctx {}.{} ch{} base-p{}", found.size(), plugin, manga,
                                  *chapter_number, pos.page + 1);
                    prefetch.nudge(found, {plugin, manga, int(*chapter_number), pos.page + 1});
                } else {
                    spdlog::debug("nudge {} urls, no chapter number -> sha fallback", found.size());
                    prefetch.nudge(found); // background delta fetch (3.3)
                }
            }

            crow::json::wvalue out;
            out["window"] = std::move(window);
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });
}

} // namespace api
} // namespace http
