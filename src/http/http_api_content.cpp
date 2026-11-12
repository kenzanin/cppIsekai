#include "api_ctx.hpp"

#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

// cache-full check for one chapter: <root>/img/<plugin>/manga/<slug>/<chdir>/pNNN.ext
// count >= pages. ch-dir matched by chNNN prefix (collision id8 variant counts too).
bool chapter_cached(const std::string& plugin, const std::string& slug, int chapter, int pages) {
    if (chapter <= 0 || pages <= 0)
        return false;
    const fs::path base = fs::path("data/cache") / "img" / plugin / "manga" / slug;
    const std::string prefix = "ch" +
                               (chapter < 10    ? std::string("00")
                                : chapter < 100 ? std::string("0")
                                                : std::string()) +
                               std::to_string(chapter);
    std::error_code ec;
    if (!fs::is_directory(base, ec))
        return false;
    for (const auto& ch : fs::directory_iterator(base, ec)) {
        if (ec)
            return false;
        if (!ch.is_directory(ec))
            continue;
        if (ch.path().filename().string().rfind(prefix, 0) != 0)
            continue;
        int n = 0;
        for (const auto& p : fs::directory_iterator(ch.path(), ec)) {
            if (!ec && p.is_regular_file(ec))
                ++n;
        }
        return n >= pages;
    }
    return false;
}

} // namespace

namespace http {
namespace api {

// Content-serving routes: the reader page list, the pass-through image cache
// and the CBZ export (split from the http_api monolith; bodies verbatim).
void register_content(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& images = c.images;

    // reader-page D1: ordered page URL list for the reader UI. Pages through
    // the stale layer; prev/next chapter resolved from the chapter list by
    // number so the reader can walk chapters without refetching the whole
    // list. Each entry is a host-rendered /cache/img URL (client never
    // builds cache paths -- design D1: ratio suffixes, v3 rel_path and
    // host-side transforms stay host-owned).
    CROW_ROUTE(app, "/api/pages/<string>/<string>/<double>")
    ([&](const crow::request& /*req*/, std::string plugin, std::string slug, double chd) -> crow::response {
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        const bool enabled = on_pool(db, [&] { return store.plugin_enabled(plugin); });
        if (!enabled) {
            crow::response r{R"({"ok":false,"error":"plugin disabled"})"};
            r.set_header("Content-Type", "application/json");
            return r;
        }
        crow::json::wvalue q;
        q["id"] = slug;
        const std::string chs_params = q.dump();
        const std::string body = on_pool(plugin_pool, [&] {
            const std::string chs_env = call_cached(store, plugin, "chapters", chs_params,
                                                    [&] { return plugins.call(plugin, "chapters", chs_params); });
            // Pre-resolve the chapter id from the list so the pages call
            // targets the right one even when the plugin's list is stale.
            std::string chapter_id;
            if (const auto chs = envelope_data(chs_env)) {
                for (const auto& item : *chs) {
                    if (!item.is_object())
                        continue;
                    const auto& entry = item.get_object();
                    const auto num = entry.find("number");
                    if (num == entry.end())
                        continue;
                    const auto* d = std::get_if<double>(&num->second.data);
                    if (!d || *d != chd)
                        continue;
                    const auto idf = entry.find("id");
                    if (idf != entry.end()) {
                        if (const auto* s = std::get_if<std::string>(&idf->second.data)) {
                            chapter_id = *s;
                            break;
                        }
                    }
                }
            }
            if (chapter_id.empty())
                return std::string{};
            crow::json::wvalue pages_q;
            pages_q["id"] = chapter_id;
            const std::string pages_params = pages_q.dump();
            const std::string pages_env = call_cached(store, plugin, "pages", pages_params,
                                                      [&] { return plugins.call(plugin, "pages", pages_params); });
            return build_pages_response(chs_env, pages_env, PagesCtx{plugin, slug, chd}).value_or(std::string{});
        });
        crow::response r;
        if (body.empty()) {
            r.code = 404;
            r.body = R"({"ok":false,"error":"unknown chapter"})";
        } else {
            r.body = std::move(body);
        }
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // Serve-through image cache: hit = disk, miss+online = fetch+store+serve,
    // miss+offline = 404. Mime from the stored file's extension.
    // V3 (cache-layout-v3 D6): optional context params (plugin, slug, ch,
    // page, cover) drive the layout-specific landing path; a client-supplied
    // ratio is ignored — host sidecar is the source of truth (D3).
    CROW_ROUTE(app, "/cache/img")
    ([&](const crow::request& req) {
        const auto url = param(req, "url");
        const bool cover = param(req, "cover") == "1" || param(req, "cover") == "true";
        const auto plugin = param(req, "plugin");
        const auto slug = param(req, "slug");
        const int ch = std::atoi(param(req, "ch").c_str());
        const int page = std::atoi(param(req, "page").c_str());
        cache::Cache::CoverCtx cctx;
        cache::Cache::PageCtx pctx;
        if (cover) {
            cctx.cover = true;
            cctx.plugin = plugin;
            cctx.url = url;
        } else if (!plugin.empty() && !slug.empty() && ch >= 1 && page >= 1) {
            pctx = {plugin, slug, ch, page};
        }
        const ImgReply r = serve_cached_image(images, url, cctx, pctx);
        crow::response res;
        res.code = r.code;
        res.body = std::move(r.body);
        res.compressed = false; // images are already compressed bytes
        res.set_header("Content-Type", std::string(r.mime));
        if (r.ratio_header)
            res.set_header("X-Image-Ratio", *r.ratio_header);
        return res;
    });

    // cache-full probe for the detail page CBZ button: per-chapter gate.
    // Query: ?plugin=&slug=&ch=<n>&pages=<total> -> {"ok":true,"full":bool}
    CROW_ROUTE(app, "/api/cbz-ready")
    ([](const crow::request& req) {
        const auto plugin = param(req, "plugin");
        const auto slug = param(req, "slug");
        const int ch = std::atoi(param(req, "ch").c_str());
        const int pages = std::atoi(param(req, "pages").c_str());
        const bool full = valid_title_pair(plugin, slug) && chapter_cached(plugin, slug, ch, pages);
        crow::json::wvalue out;
        out["ok"] = true;
        out["full"] = full;
        crow::response r{out};
        r.set_header("Content-Type", "application/json");
        return r;
    });

    // CBZ export (cache-layout D6): stream the cached tree as an in-memory zip;
    // empty/unknown slug = empty-but-valid zip, never 404.
    CROW_ROUTE(app, "/export/cbz")
    ([](const crow::request& req) {
        crow::response res;
        const auto zip = cbz::build("data/cache", param(req, "plugin"), param(req, "slug"));
        if (!zip) {
            res.code = 500;
            res.set_header("Content-Type", "text/plain");
            res.body = "export failed";
            return res;
        }
        res.body = std::move(*zip);
        res.compressed = false; // zip container, already deflate-packed
        res.set_header("Content-Type", "application/zip");
        res.set_header("Content-Disposition", "attachment; filename=\"" + param(req, "slug") + ".cbz\"");
        return res;
    });
}

} // namespace api
} // namespace http
