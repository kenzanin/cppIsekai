#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <utility>

namespace fs = std::filesystem;

#include "http_content_internal.hpp"

namespace http::api {

// Content-serving routes: the reader page list, the pass-through image cache
// and the CBZ export (split from the http_api monolith; bodies verbatim).
void register_content(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;
    auto& images = c.images;

    // reader-page D1: ordered page URL list for the reader UI. Pages through
    // the stale layer; prev/next chapter resolved from a number-stable-sorted
    // copy of the list (design D3) so duplicate releases stay reachable. Each
    // entry is a host-rendered /cache/img URL (client never builds cache
    // paths -- design D1: ratio suffixes, v3 rel_path and host-side transforms
    // stay host-owned). An optional `ch_id` query param picks the exact release.
    CROW_ROUTE(app, "/api/pages/<string>/<string>/<double>")
    ([&](const crow::request& req, std::string plugin, std::string slug, double chd) -> crow::response {
        if (!valid_title_pair(plugin, slug)) {
            return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
        }
        const std::string want_id = param(req, "ch_id"); // empty = number-only legacy path (design D1)
        // Skip-flagged chapters are hidden from the list, so the reader must not
        // walk into them either: prev/next resolution steps over these ids.
        std::set<std::string> hidden;
        for (const auto& [id, flags] : store.chapter_flags_list(plugin, slug)) {
            if ((flags & std::to_underlying(ChapterFlag::kSkip)) != 0) {
                hidden.insert(id);
            }
        }
        crow::json::wvalue q;
        q["id"] = slug;
        const std::string chs_params = q.dump();
        // page-url rows carry the release's pages; no host-side total copy.
        std::string pages_error;
        const std::string body = on_pool(plugin_pool, [&] {
            const std::string chs_env = call_cached(store, plugin, "chapters", chs_params,
                                                    [&] { return plugins.call(plugin, "chapters", chs_params); }, &plugins);
            // Pre-resolve the chapter id from the list so the pages call
            // targets the right one even when the plugin's list is stale.
            // A `ch_id` query param wins when it matches an entry; otherwise
            // the first number match is used (design D1/D2).
            const std::string chapter_id = resolve_chapter_id(chs_env, want_id, chd);
            if (chapter_id.empty()) {
                return std::string{};
            }
            // Walk the primary id then same-number siblings; first release
            // whose pages parse wins (per-entry upstream flakes, comico 502s).
            std::vector<std::string> candidates = sibling_ids(chs_env, chd, chapter_id);
            candidates.insert(candidates.begin(), chapter_id);
            std::string pages_id = chapter_id;
            std::string pages_env;
            for (const auto& cand : candidates) {
                crow::json::wvalue pages_q;
                pages_q["id"] = cand;
                const std::string pages_params = pages_q.dump();
                pages_env = call_cached(store, plugin, "pages", pages_params,
                                        [&] { return plugins.call(plugin, "pages", pages_params); }, &plugins);
                if (const auto pages = envelope_data(pages_env); pages && !pages->empty()) {
                    pages_id = cand;
                    // chapter-store 2.2: page-url rows ride every parse (D4
                    // replace-all); the reader-visible list stays the same shape.
                    std::vector<std::string> urls;
                    urls.reserve(pages->size());
                    for (const auto& el : *pages) {
                        if (const auto* s = std::get_if<std::string>(&el.data)) {
                            urls.emplace_back(*s);
                        }
                    }
                    store.chapter_pages_replace(plugin, slug, pages_id, urls);
                    break;
                }
                if (std::string e = envelope_error(pages_env); !e.empty()) {
                    pages_error = std::move(e); // keep the real upstream reason for the 404 body
                }
            }
            return build_pages_response(chs_env, pages_env,
                                        PagesCtx{.plugin = plugin,
                                                 .slug = slug,
                                                 .ch = chd,
                                                 .ch_id = pages_id,
                                                 .hidden = hidden})
                .value_or(std::string{});
        });
        crow::response r;
        if (body.empty()) {
            r.code = 404;
            if (pages_error.empty()) {
                r.body = R"({"ok":false,"error":"unknown chapter"})";
            } else {
                crow::json::wvalue err;
                err["ok"] = false;
                err["error"] = pages_error;
                r.body = err.dump();
            }
        } else {
            r.body = body;
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
        // crow's query parser truncates a value at a nested '?' (proxy urls
        // like .../img?url=<enc> lose their tail), so take url verbatim from
        // raw_url: everything after "url=" up to the next '&' (D-none: hosts
        // with nested-query proxies, 4ucomics api/v1/img).
        std::string url{param(req, "url")};
        if (const auto raw = req.raw_url; !raw.empty()) {
            if (const auto q = raw.find("?url="); q != std::string::npos) {
                auto end = raw.find('&', q + 5);
                url = pct_decode(raw.substr(q + 5, end == std::string::npos ? std::string::npos : end - (q + 5)));
            }
        }
        const bool cover = param(req, "cover") == "1" || param(req, "cover") == "true";
        const auto plugin = param(req, "plugin");
        const auto slug = param(req, "slug");
        const int ch = parse::to_int(param(req, "ch"));
        const int page = parse::to_int(param(req, "page"));
        const std::string ch_id = param(req, "ch_id");
        cache::Cache::CoverCtx cctx;
        cache::Cache::PageCtx pctx;
        if (cover) {
            cctx.cover = true;
            cctx.plugin = plugin;
            cctx.url = url;
        } else if (!plugin.empty() && !slug.empty() && ch >= 0 && page >= 1) { // ch 0 = prologue (comico)
            pctx = {.plugin = plugin, .slug = slug, .chapter = ch, .page = page, .chapter_id = ch_id};
        }
        const ImgReply r = serve_cached_image(images, url, cctx, pctx);
        crow::response res;
        res.code = r.code;
        res.body = r.body;
        res.compressed = false; // images are already compressed bytes
        res.set_header("Content-Type", std::string(r.mime));
        if (r.ratio_header) {
            res.set_header("X-Image-Ratio", *r.ratio_header);
        }
        return res;
    });

    // cache-full probe for the detail page CBZ button: per-chapter gate.
    // Query: ?plugin=&slug=&ch=<n>&pages=<total> -> {"ok":true,"full":bool}
    CROW_ROUTE(app, "/api/cbz-ready")
    ([&](const crow::request& req) {
        const auto plugin = param(req, "plugin");
        const auto slug = param(req, "slug");
        const int ch = parse::to_int(param(req, "ch"));
        const int pages = parse::to_int(param(req, "pages"));
        const bool full = valid_title_pair(plugin, slug) && chapter_cached(images.ns(plugin), slug, ch, pages);
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
    ([&](const crow::request& req) {
        crow::response res;
        const auto zip = cbz::build("data/cache", images.ns(param(req, "plugin")), param(req, "slug"));
        if (!zip) {
            res.code = 500;
            res.set_header("Content-Type", "text/plain");
            res.body = "export failed";
            return res;
        }
        res.body = *zip;
        res.compressed = false; // zip container, already deflate-packed
        res.set_header("Content-Type", "application/zip");
        res.set_header("Content-Disposition", "attachment; filename=\"" + param(req, "slug") + ".cbz\"");
        return res;
    });
}

} // namespace http::api
