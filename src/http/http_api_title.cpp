#include "api_ctx.hpp"
#include "enrich_internal.hpp"

#include <ctime>

namespace http::api {
namespace {

// The shared body of the detail/chapters endpoints: disabled = cache-only via
// the stale layer, json envelope. `method` is the plugin method; `id_key` the query param the
// plugin's contract names for the title key.
//
// manga-detail-table: a saved row with materialized detail answers /api/detail
// from the row (D3) -- no plugin call, `cached` flagged (D8), enrichment overlay
// intact (D7). Otherwise the live path runs and the resolved fields are written
// back through materialize_detail (D2, route-level -- never inside apply_chosen).
crow::response title_data_response(plugin::Host& plugins, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                   Store& store, const std::string& plugin, const std::string& slug,
                                   const std::string& method) {
    if (!valid_title_pair(plugin, slug)) {
        return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
    }
    crow::json::wvalue p;
    p["id"] = slug; // the slug IS the title key the cache layout and plugins mint
    const std::string params = p.dump();

    std::string body = method == "detail" ? saved_detail_response(store, db, plugin, slug) : std::string{}; // '' unless the row serves
    if (method == "chapters") {
        // library-promote-on-save D3: stored rows answer first; '' = cold, the
        // cached-or-live path below runs and materializes on the way out.
        body = chapters_row_response(store, db, plugin, slug);
    }
    const bool row_served = !body.empty();
    if (!row_served) {
        body = on_pool(plugin_pool, [&] {
            return call_cached(store, plugin, method, params, [&] { return plugins.call(plugin, method, params); }, &plugins);
        });
    }
    if (method == "detail") { // chosen rides BOTH paths: newer picker edits must win over the row
        body = apply_chosen(store, db, plugin, slug, body);
    }
    if (method == "detail") {
        // library-grid-stats: the read rollup is pure-DB compute -- run it on
        // both paths so a cached (row-served) open keeps read_ch/Done fresh.
        const std::string chs_body = on_pool(db, [&] {
            if (auto row = store.api_cache_get(plugin, stale_key(plugin, "chapters", params))) {
                return row->payload;
            }
            return std::string{};
        });
        if (!row_served) {
            materialize_detail(store, db, plugin, slug, body); // write-through (D2, post-merge)
            // library-grid-stats 2.2: saved-manga stat sync on detail open.
            // Reuse the stale-served envelopes already in api_cache (no extra
            // network); failures leave the last-known stats untouched. A row-
            // served answer learned nothing new -- the sync would only feed the
            // display synopsis back into the source column (D4), so it skips.
            sync_saved_stats(store, db, plugin, slug, body, chs_body);
        } else {
            apply_read_rollup(store, db, plugin, slug, chs_body); // row-serve: stats only
        }
        // library-grid-stats: detail open is the writer of status_flags (via the
        // rollup above); echo the stored bitfield back so the page badges read
        // exactly what the library grid renders.
        const int flags = on_pool(db, [&] {
            const auto row = store.saved_get(plugin, slug);
            return row ? row->status_flags : 0;
        });
        body = detail::inject_status_flags(body, flags);
    }
    if (method == "chapters") {
        // chapter-store 2.1: every chapters parse materializes identity rows
        // (the UI's canonical list path; flags/legacy-inherit ride inside).
        materialize_chapters(store, db, plugin, slug, body);
    }
    crow::response r{std::move(body)};
    r.set_header("Content-Type", "application/json");
    return r;
}

} // namespace

// manga-detail 1.1 detail data + related + chapters + forced refresh
// (split from the http_api monolith; bodies verbatim).
void register_title(Ctx& c) {
    auto& app = c.app;
    auto& store = c.store;
    auto& db = c.db;
    auto& plugin_pool = c.plugin_pool;
    auto& plugins = c.plugins;

    // manga-detail 1.1: the detail-page data endpoints. Same envelope rules as
    // /plugin/<id>/<method>; keyed by our own slug, which is the title key the
    // cache layout and plugins mint. The page's Alpine layer consumes these.
    CROW_ROUTE(app, "/api/detail/<string>/<string>")
    ([&](const crow::request& /*req*/, const std::string& plugin, const std::string& slug) {
        // sync/flags/materialize/read-first live in title_data_response (shared
        // body above); the route is the thin mapper it always was.
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "detail");
    });

    // detail-enrichment 2.1: related readings; same envelope and stale-serve
    // rules as /api/detail, plugin method "related".
    CROW_ROUTE(app, "/api/related/<string>/<string>")
    ([&](const crow::request& /*req*/, const std::string& plugin, const std::string& slug) {
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "related");
    });

    CROW_ROUTE(app, "/api/chapters/<string>/<string>")
    ([&](const crow::request& /*req*/, const std::string& plugin, const std::string& slug) {
        return title_data_response(plugins, db, plugin_pool, store, plugin, slug, "chapters");
    });

    // manga-detail 1.2: forced refresh (design D3). Call-through both methods,
    // overwrite the stale entries on success; the first failure returns it and
    // leaves every cached entry untouched.
    CROW_ROUTE(app, "/api/refresh/<string>/<string>")
        .methods(crow::HTTPMethod::POST)(
            [&](const crow::request& /*req*/, const std::string& plugin, const std::string& slug) -> crow::response {
                const RefreshOutcome rr = refresh_title(store, db, plugin_pool, plugins, plugin, slug);
                auto json = [&](int code, const std::string& body) {
                    crow::response r{code, body};
                    r.set_header("Content-Type", "application/json");
                    return r;
                };
                switch (rr.result) {
                case RefreshResult::kRejected:
                    return crow::response{404, R"({"ok":false,"error":"unknown title"})"};
                case RefreshResult::kDisabled:
                    return json(200, R"({"ok":false,"error":"plugin disabled"})");
                case RefreshResult::kOrphan:
                    return json(200, R"({"ok":false,"error":"plugin removed"})");
                case RefreshResult::kUpstreamFailed: {
                    const std::string body =
                        std::string(R"({"ok":false,"error":"refresh failed: )") + rr.failed_method + "\"}";
                    return json(502, body); // upstream plugin call failed, not the route
                }
                case RefreshResult::kOk:
                    return json(200, R"({"ok":true})");
                }
                return json(500, R"({"ok":false,"error":"refresh failed"})");
            });
}

} // namespace http::api
