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

// Per-chapter host flags (skip / read progress / batched state) -- the
// reader-window rerun on skip keeps the response honest (bodies verbatim
// from the http_api monolith).
void register_state(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& images = c.images;
    auto& prefetch = c.prefetch;

    // Per-manga dim+blur cover lives on the saved row itself: status_flags
    // bit6 (is_dim), toggled from the detail page, applied to every cover
    // img that carries plugin+slug (grid/detail). Unsaved manga have no row,
    // so the toggle is a no-op there (the request still succeeds).
    CROW_ROUTE(app, "/api/display/dim-blur")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            if (!body || !body.has("plugin") || !body.has("slug")) {
                return crow::response{400};
            }
            const std::string plugin = body["plugin"].s();
            const std::string slug = body["slug"].s();
            if (plugin.empty() || plugin.contains('/') || slug.empty()) {
                return crow::response{400};
            }
            bool on = false;
            if (body.has("on")) {
                on = body["on"].t() == crow::json::type::True;
            }
            on_pool(db, [&] {
                store.saved_dim_update(plugin, slug, on);
                return 0;
            });
            crow::response r{on ? R"({"ok":true,"on":true})" : R"({"ok":true,"on":false})"};
            r.set_header("Content-Type", "application/json");
            return r;
        });
    // GET list of dimmed manga: [["plugin","slug"], ...] for client-side paint.
    CROW_ROUTE(app, "/api/display/dim-list")([&] {
        const auto pairs = on_pool(db, [&] { return store.saved_dim_list(); });
        crow::json::wvalue list(crow::json::type::List);
        int idx = 0;
        for (const auto& [plugin, slug] : pairs) {
            crow::json::wvalue pair(crow::json::type::List);
            pair[0] = plugin;
            pair[1] = slug;
            list[idx++] = std::move(pair);
        }
        crow::json::wvalue out;
        out["ok"] = true;
        out["data"] = std::move(list);
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Chapter skip (image-cache): the flag lands under reserved
    // host/skip/<uuid>; the window re-runs against the last position so the
    // response already reflects the reroute.
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
                store.state_set(kStateHost, std::string(kSkipPrefix) + id, skipped ? "1" : "0");
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
                store.state_set(kStateHost, std::string(kReadPrefix) + id, std::to_string(page));
                if (!plugin.empty() && !slug.empty()) {
                    if (page <= 0) {
                        store.chapter_flags_apply(plugin, slug, {id}, false, 0,
                                                  std::to_underlying(ChapterFlag::kRead));
                    } else if (const int total = store.chapter_pages_count(plugin, slug, id);
                               total > 0 && page >= total) {
                        store.chapter_flags_apply(plugin, slug, {id}, false,
                                                  std::to_underlying(ChapterFlag::kRead), 0);
                        done = true;
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

    // chapter-store 3.1: one generic bits endpoint -- set/clear any flag
    // combination over ids or the whole manga; range logic stays client-side
    // (D3, Suwayomi updateChapters parity).
    CROW_ROUTE(app, "/chapter/flags")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
            const auto body = crow::json::load(req.body);
            std::vector<std::string> ids;
            bool all = false;
            int set = 0, clear = 0;
            std::string plugin, slug;
            if (body) {
                if (body.has("ids") && body["ids"].t() == crow::json::type::List) {
                    for (const auto& v : body["ids"]) {
                        if (v.t() == crow::json::type::String) {
                            ids.push_back(v.s());
                        }
                    }
                }
                if (body.has("all") && body["all"].t() == crow::json::type::True) {
                    all = true;
                }
                if (body.has("set") && body["set"].t() == crow::json::type::Number) {
                    set = static_cast<int>(body["set"].i());
                }
                if (body.has("clear") && body["clear"].t() == crow::json::type::Number) {
                    clear = static_cast<int>(body["clear"].i());
                }
                if (body.has("plugin") && body["plugin"].t() == crow::json::type::String) {
                    plugin = body["plugin"].s();
                }
                if (body.has("slug") && body["slug"].t() == crow::json::type::String) {
                    slug = body["slug"].s();
                }
            }
            if (plugin.empty() || plugin.contains('/') || slug.empty() || slug.contains('/') || (!all && ids.empty())) {
                crow::response r{R"({"ok":false,"error":"plugin, slug and ids|all are required"})"};
                r.code = 400;
                r.set_header("Content-Type", "application/json");
                return r;
            }
            crow::json::wvalue out;
            auto touched = on_pool(db, [&] { return store.chapter_flags_apply(plugin, slug, ids, all, set, clear); });
            out["ok"] = true;
            crow::json::wvalue flags(crow::json::type::Object);
            for (const auto& [cid, f] : touched) {
                flags[cid] = f;
            }
            out["flags"] = std::move(flags);
            crow::response r{out};
            r.set_header("Content-Type", "application/json");
            return r;
        });

    // Batch read/skip state (chapter-list-controls 1.2): one round trip for
    // the detail page list. Absent keys omit the field.
    CROW_ROUTE(app, "/chapter/state").methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response {
        const auto body = crow::json::load(req.body);
        std::vector<std::string> ids;
        std::string plugin, slug;
        if (body && body.has("ids") && body["ids"].t() == crow::json::type::List) {
            for (const auto& v : body["ids"]) {
                if (v.t() == crow::json::type::String) {
                    ids.push_back(v.s());
                }
            }
        }
        if (body) {
            if (body.has("plugin") && body["plugin"].t() == crow::json::type::String) {
                plugin = body["plugin"].s();
            }
            if (body.has("slug") && body["slug"].t() == crow::json::type::String) {
                slug = body["slug"].s();
            }
        }
        // One walk of the v3 chapter tree: dir name -> file count. Duplicate
        // releases land in sibling dirs (chNNN vs chNNN-id8suffix);
        // number-only dirs can't be matched to an id, so only the
        // suffix-shaped dirs are reported here.
        std::map<std::string, int> cached_dirs;
        if (!plugin.empty() && !plugin.contains('/') && !slug.empty() && !slug.contains('/')) {
            const fs::path base = fs::path("data/cache") / "img" / images.ns(plugin) / "manga" / cache::slug_dir(slug);
            std::error_code ec;
            if (fs::is_directory(base, ec)) {
                for (const auto& e : fs::directory_iterator(base, ec)) {
                    if (ec) {
                        break;
                    }
                    if (!e.is_directory(ec)) {
                        continue;
                    }
                    const std::string dir = e.path().filename().string();
                    if (!dir.starts_with("ch")) {
                        continue;
                    }
                    int n = 0;
                    for (const auto& p : fs::directory_iterator(e.path(), ec)) {
                        if (!ec && p.is_regular_file(ec)) {
                            ++n;
                        }
                    }
                    if (n > 0) {
                        cached_dirs[dir] = n;
                    }
                }
            }
        }
        // last-8 of the id == the cache-dir suffix (page_rel). Match dirs of
        // the shape chNNN-<suffix> against it.
        const auto cached_for = [&cached_dirs](const std::string& id) -> std::optional<int> {
            const std::string suffix = id.size() <= 8 ? id : id.substr(id.size() - 8);
            int best = -1;
            for (const auto& [dir, n] : cached_dirs) {
                if (dir.size() > suffix.size() + 1 && dir.ends_with(suffix) &&
                    dir[dir.size() - suffix.size() - 1] == '-') {
                    best = std::max(best, n);
                }
            }
            return best >= 0 ? std::optional<int>(best) : std::nullopt;
        };
        crow::json::wvalue out;
        on_pool(db, [&] {
            // chapter-store 3.2: skip/read DONE come from the flags bitfield
            // (single truth); progress/total/cached stay key/table derived.
            const std::map<std::string, int> flags =
                plugin.empty() ? std::map<std::string, int>{} : store.chapter_flags_list(plugin, slug);
            for (const auto& id : ids) {
                if (const auto f = flags.find(id); f != flags.end()) {
                    if (f->second & std::to_underlying(ChapterFlag::kSkip)) {
                        out[id]["skip"] = true;
                    }
                    if (f->second & std::to_underlying(ChapterFlag::kRead)) {
                        out[id]["done"] = true;
                    }
                }
                if (const auto r = store.state_get(kStateHost, std::string(kReadPrefix) + id);
                    r && !r->empty() && *r != "0") {
                    out[id]["read"] = parse::to_int(*r);
                }
                if (const auto t = store.state_get(kStateHost, std::string(kTotalPrefix) + id);
                    t && !t->empty() && *t != "0") {
                    out[id]["total"] = parse::to_int(*t);
                }
                if (const auto c2 = cached_for(id); c2 && *c2 > 0) {
                    out[id]["cached"] = *c2;
                }
            }
            return 0;
        });
        crow::response res{out};
        res.set_header("Content-Type", "application/json");
        return res;
    });
}

} // namespace http::api
