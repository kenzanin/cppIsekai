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

void register_state_state(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& images = c.images;

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
            // chapter-store 3.2: skip/read DONE come from the flags bitfield,
            // progress from the master row, total from the pages table -- the
            // legacy host shadows are gone.
            const std::map<std::string, int> flags =
                plugin.empty() ? std::map<std::string, int>{} : store.chapter_flags_list(plugin, slug);
            const std::map<std::string, int> read_max =
                plugin.empty() ? std::map<std::string, int>{} : store.chapter_read_max_list(plugin, slug);
            for (const auto& id : ids) {
                if (const auto f = flags.find(id); f != flags.end()) {
                    if (f->second & std::to_underlying(ChapterFlag::kSkip)) {
                        out[id]["skip"] = true;
                    }
                    if (f->second & std::to_underlying(ChapterFlag::kRead)) {
                        out[id]["done"] = true;
                    }
                }
                if (const auto r = read_max.find(id); r != read_max.end() && r->second > 0) {
                    out[id]["read"] = r->second;
                }
                if (const int t = plugin.empty() ? 0 : store.chapter_pages_count(plugin, slug, id); t > 0) {
                    out[id]["total"] = t;
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
