#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "plugin/url_codec.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace http::api {

void register_state_flags(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& prefetch = c.prefetch;

    CROW_ROUTE(app, "/chapter/<string>/skip")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req, std::string id) -> crow::response {
            id = plugin::url_decode(id); // crow leaves %3A etc. encoded in path params
            bool skipped = false;
            std::string plugin, slug;
            if (const auto body = crow::json::load(req.body); body) {
                if (body.has("skipped")) {
                    const auto t = body["skipped"].t();
                    if (t == crow::json::type::True || t == crow::json::type::False) {
                        skipped = body["skipped"].b();
                    }
                }
                // chapter-store 3.4: identity-carrying callers land the bit
                // directly; the key write below stays the legacy record.
                if (body.has("plugin") && body["plugin"].t() == crow::json::type::String) {
                    plugin = body["plugin"].s();
                }
                if (body.has("slug") && body["slug"].t() == crow::json::type::String) {
                    slug = body["slug"].s();
                }
            }

            on_pool(db, [&] {
                if (!plugin.empty() && !slug.empty()) {
                    if (skipped) {
                        store.chapter_flags_apply(plugin, slug, {id}, false, std::to_underlying(ChapterFlag::kSkip), 0);
                    } else {
                        store.chapter_flags_apply(plugin, slug, {id}, false, 0, std::to_underlying(ChapterFlag::kSkip));
                    }
                }
                return 0;
            });

            std::vector<crow::json::wvalue> window;
            const std::optional<ReaderPosition> last = on_pool(db, [&] { return store.position_last(); });
            if (last) {
                const std::string& plugin = last->plugin;
                const std::string& manga = last->slug;
                const std::string& chapter = last->chapter_id;
                if (!plugin.empty() && !manga.empty() && !chapter.empty()) {
                    const reader::Position where{.chapter_id = chapter, .page = last->page};
                    const std::vector<reader::WindowPart> found =
                        reader_window(store, db, plugin_pool, plugins, plugin, manga, where);
                    for (const auto& part : found) {
                        for (const auto& url : part.urls) {
                            window.emplace_back(url);
                        }
                    }
                    if (!found.empty()) {
                        // Per-part placement, mirroring the reader route's nudge:
                        // each slice lands in its own chapter dir (rerun delta 3.3).
                        prefetch.nudge(found, plugin, manga);
                    }
                }
            }

            crow::json::wvalue out;
            out["id"] = id;
            out["skipped"] = skipped;
            out["window"] = std::move(window);
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // Chapter read progress (chapter-list-controls 1.1): max pages read per
    // chapter under host/read/<uuid>; page 0 clears the marker. Callers that
    // also send plugin/slug get the host-set read bit at the same threshold
    // as the position path (chapter-store 3.3 / D5); page 0 clears it.
    CROW_ROUTE(app, "/chapter/<string>/read")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req, std::string id) -> crow::response {
            id = plugin::url_decode(id); // keep state keys identical to the reader/body paths
            const auto body = crow::json::load(req.body);
            const int page = static_cast<int>(std::max(0L, num_field(body, "page", 0)));
            std::string plugin, slug;
            if (body) {
                if (body.has("plugin") && body["plugin"].t() == crow::json::type::String) {
                    plugin = body["plugin"].s();
                }
                if (body.has("slug") && body["slug"].t() == crow::json::type::String) {
                    slug = body["slug"].s();
                }
            }
            bool done = false;
            on_pool(db, [&] {
                if (!plugin.empty() && !slug.empty()) {
                    if (page <= 0) {
                        store.chapter_flags_apply(plugin, slug, {id}, false, 0,
                                                  std::to_underlying(ChapterFlag::kRead));
                    } else {
                        // read progress lives on the master row (read_max); the
                        // bit flips host-side at the stored page count.
                        store.chapter_read_max_bump(plugin, slug, id, page);
                        if (const int total = store.chapter_pages_count(plugin, slug, id); total > 0 && page >= total) {
                            done = true;
                        }
                    }
                }
                return 0;
            });
            crow::json::wvalue out;
            out["id"] = id;
            out["read"] = page;
            out["done"] = done;
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

}

} // namespace http::api
