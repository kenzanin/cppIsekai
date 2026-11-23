#pragma once

/** \file api_ctx.hpp
 *  \brief Internal shared context + helpers for the http_api route TUs.
 *  Monolith split (compile parallelism, <=~200 loc per unit): one Ctx of
 *  service refs handed to every register_*; state keys, body field readers
 *  and the title-pair guard live here once. Not a public http surface.
 */

#include "http_routes.hpp"
#include "stale.hpp"

#include "config/config.hpp"
#include "store/store.hpp"

#include "position_api.hpp"
#include "reader/window.hpp"

#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace http::api {

/** Service handles every route group needs; built once in register_api. */
struct Ctx {
    crow::SimpleApp& app;
    Store& store;
    BS::thread_pool<>& db;
    BS::thread_pool<>& plugin_pool;
    plugin::Host& plugins;
    cache::Cache& images;
    reader::Worker& prefetch;
    reader::CoverWorker& covers;
    const std::string& ini_path;
    const config::Overrides& overrides;
};

// Reserved plugin_state rows for host bookkeeping: the last reader position
// (the anchor the skip-rerun reads back) and per-pair detail flags.
inline constexpr const char* kStateHost = "host";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.
inline std::string str_field(const crow::json::rvalue& j, const char* key) {
    return j.has(key) && j[key].t() == crow::json::type::String ? std::string(j[key].s()) : std::string{};
}
inline long num_field(const crow::json::rvalue& j, const char* key, long def) {
    return j.has(key) && j[key].t() == crow::json::type::Number ? j[key].i() : def;
}

// manga-detail 1.1: the {plugin, slug} path pair guard. Slugs are our own
// cache-layout keys; the whitelist used to be [a-zA-Z0-9-_.] but real-world slugs
// carry unicode/apostrophes/exclamation marks. The guard now denies only
// path-significant or control bytes and dot segments: `/` `\` `?` `#` `%`,
// space, C0/DEL controls; `..` is always refused. Returns false when the pair
// needs the not-found response.
inline bool valid_title_pair(const std::string& plugin, const std::string& slug) {
    const auto ok = [](const std::string& s) {
        // The slug IS the site-minted id (4ucomics: 202 chars) and doubles as the
        // per-title cache dir component, so the ceiling is the filesystem's
        // NAME_MAX, not a display length.
        if (s.empty() || s.size() > 255) {
            return false;
        }
        for (const unsigned char c : s) {
            if (c == '/' || c == '\\' || c == '?' || c == '#' || c == '%' || c == 0x20 || c < 0x20 || c == 0x7f) {
                return false;
            }
        }
        if (s.contains("..")) {
            return false;
        }
        return true;
    };
    return ok(plugin) && ok(slug);
}

// The reader window composition (image-cache): chapter list + pages through
// the cached ABI path (stale-serve included), skip flags from plugin_state,
// math in reader::window_parts. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
// Defined in http_api_reader.cpp; the state TU reuses it for the skip-rerun.
// Per-chapter parts: the prefetch nudge needs them; flatten for the response.
std::vector<reader::WindowPart> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                              plugin::Host& plugins, const std::string& plugin_id,
                                              const std::string& manga_id, const reader::Position& pos);

// Route-group registration, one TU each (http_api_*.cpp).
void register_admin(Ctx& /*c*/); // plugin registry/admin + config endpoints
void register_library(Ctx& /*c*/); // saved-library CRUD
void register_title(Ctx& /*c*/);   // detail/related/chapters/refresh envelopes
void register_reader(Ctx& /*c*/); // reader window + position routes
void register_state(Ctx& /*c*/); // chapter skip/read/state flags
void register_content(Ctx& /*c*/); // reader pages, image cache, cbz export
void register_enrich(Ctx& /*c*/); // enrichment fetch/chosen/reset

// manga-detail-table D2: forward decls so refresh_title (below, parsed before or
// after enrich_internal depending on the TU include order) links to the
// definitions in enrich_internal.hpp.
void materialize_detail(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                        const std::string& resolved_env);
std::string apply_chosen(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                         const std::string& envelope);

} // namespace http::api

// manga-detail-table D2: refresh_title (below) materializes through the enrichment
// helpers; placed after the decls above because enrich_internal re-opens this
// namespace and needs valid_title_pair visible (pragma-once makes the cycle benign).
#include "enrich_internal.hpp"

// --- saved-manga stats sync (library-grid-stats 2.x) ---
namespace detail {
inline std::optional<glz::generic::object_t> envelope_object(const std::string& env) {
    glz::generic parsed;
    if (glz::read_json(parsed, env) || !envelope_ok(parsed) || !parsed.is_object()) {
        return std::nullopt;
    }
    auto* const data = parsed.get_object().find("data");
    if (data == parsed.get_object().end() || !data->second.is_object()) {
        return std::nullopt;
    }
    return data->second.get_object();
}
inline std::string envelope_str(const glz::generic::object_t& obj, const char* key) {
    const auto* const it = obj.find(key);
    if (it == obj.end()) {
        return {};
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data)) {
        return *s;
    }
    return {};
}
// library-dedup 1.2: alt_titles re-serialized verbatim (source envelope only)
inline std::string envelope_raw_json(const glz::generic::object_t& obj, const char* key) {
    const auto* const it = obj.find(key);
    if (it == obj.end() || it->second.is_null()) {
        return {};
    }
    return glz::write_json(it->second).value_or(std::string{});
}
// library-grid-stats: the stored status_flags bitfield (bit0=is_new, bit1=is_done)
// rides into the detail envelope's `data`, so the page badges read the same
// stored value the library grid renders. Detail open is the writer; both read
// the saved-manga row.
inline std::string inject_status_flags(const std::string& envelope, int flags) {
    glz::generic env;
    if (glz::read_json(env, envelope) || !envelope_ok(env) || !env.is_object()) {
        return envelope;
    }
    auto& obj = env.get_object();
    auto* const data = obj.find("data");
    if (data == obj.end() || !data->second.is_object()) {
        return envelope;
    }
    glz::generic value;
    value.data = static_cast<double>(flags);
    data->second.get_object()["status_flags"] = std::move(value);
    return glz::write_json(env).value_or(envelope);
}
} // namespace detail

// chapter-store 2.1: materialize chapters rows from a parsed chapters
// envelope (identity upsert; flags ride through, D2 legacy keys inherit on
// first sight). No-op when the envelope is missing or carries no ids.
inline void materialize_chapters(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                 const std::string& slug, const std::string& chapters_env) {
    const auto chs = envelope_data(chapters_env);
    if (!chs) {
        return;
    }
    std::vector<ChapterSeed> seeds;
    seeds.reserve(chs->size());
    for (const auto& item : *chs) {
        if (!item.is_object()) {
            continue;
        }
        const auto& entry = item.get_object();
        const auto* const id_it = entry.find("id");
        if (id_it == entry.end()) {
            continue;
        }
        const auto* const id_s = std::get_if<std::string>(&id_it->second.data);
        if (id_s == nullptr || id_s->empty()) {
            continue;
        }
        double number = 0;
        if (const auto* const n = entry.find("number"); n != entry.end()) {
            if (const auto* d = std::get_if<double>(&n->second.data)) {
                number = *d;
            }
        }
        // release_at stored verbatim; a numeric epoch survives as its integer
        // text (detail.js fmtDate accepts both shapes).
        std::string release_at;
        if (const auto* const rel = entry.find("release_at"); rel != entry.end()) {
            if (const auto* s = std::get_if<std::string>(&rel->second.data)) {
                release_at = *s;
            } else if (const auto* d = std::get_if<double>(&rel->second.data)) {
                release_at = std::to_string(static_cast<long long>(*d));
            }
        }
        seeds.push_back(ChapterSeed{.chapter_id = *id_s,
                                    .number = number,
                                    .title = detail::envelope_str(entry, "title"),
                                    .url = detail::envelope_str(entry, "url"),
                                    .release_at = release_at});
    }
    if (!seeds.empty()) {
        http::on_pool(db, [&] {
            store.chapters_upsert(plugin, slug, seeds);
            return 0;
        });
    }
}

// chapter-store row-serve (library-promote-on-save D3): the chapters envelope
// built from stored rows; '' when the pair has none (caller falls back to
// call_cached). cached:true -- the rows ARE the local copy; a parse or refresh
// refreshes them.
inline std::string chapters_row_response(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                         const std::string& slug) {
    auto rows = http::on_pool(db, [&] { return store.chapters_list(plugin, slug); });
    if (rows.empty()) {
        return {};
    }
    glz::generic env;
    auto& root = env.data.emplace<glz::generic::object_t>();
    root["ok"].data = true;
    root["cached"].data = true;
    glz::generic::array_t arr;
    arr.reserve(rows.size());
    for (auto& r : rows) {
        glz::generic item;
        auto& o = item.data.emplace<glz::generic::object_t>();
        o["id"].data = std::move(r.chapter_id);
        o["number"].data = r.number;
        o["title"].data = std::move(r.title);
        o["url"].data = std::move(r.url);
        o["release_at"].data = std::move(r.release_at);
        // flags: the row is served local-first for saved pairs, so the UI's
        // done/new badges need the bitfield on the wire. Library-ownership
        // follow-up: /api/chapters reported no flags (DB says DONE, UI none).
        o["flags"].data = static_cast<double>(r.flags);
        arr.push_back(std::move(item));
    }
    root["data"] = std::move(arr);
    return glz::write_json(env).value_or(std::string{});
}

// Count flagged-read chapter rows and write the rollup onto the saved-manga
// row. An absent chapters envelope is NOT a reason to bail: on a row-served
// detail open the api_cache payload may be NULL/pruned while the rows still
// hold every flag. The guard is at the end -- skip only when neither the rows
// nor the envelope learned a chapter count.
inline void apply_read_rollup(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug, const std::string& chapters_env) {
    materialize_chapters(store, db, plugin, slug, chapters_env);
    const auto chs = envelope_data(chapters_env);
    // chapter-store 1.8: the read rollup counts bit0 rows (single truth), no
    // longer the per-id progress>=pages derive.
    // chapter-store 1.8: the read rollup counts bit0 rows (single truth), no
    // longer the per-id progress>=pages derive.
    int completed = 0;
    std::size_t total = 0;
    {
        std::map<std::string, int> flags;
        http::on_pool(db, [&] {
            flags = store.chapter_flags_list(plugin, slug);
            return 0;
        });
        for (const auto& [id, f] : flags) {
            if (f & std::to_underlying(ChapterFlag::kRead)) {
                ++completed;
            }
        }
        // The chapter rows are the source of truth for the total too. A
        // row-served detail open can have no chapters envelope in api_cache
        // (payload NULL/pruned); using that envelope's size left read_ch stale
        // while every row was flagged read, so Done never lit up. Only an
        // unsaved pair (flags in the ephemeral store, no rows) falls back.
        total = (flags.empty() && chs) ? chs->size() : flags.size();
        if (total == 0) {
            return; // nothing learned: no rows and no envelope
        }
    }
    // Keep total_ch fresh on row-served opens: a total learned only on the
    // live path leaves done = read_ch >= total_ch stuck at total 0.
    http::on_pool(db, [&] {
        store.saved_stats_update(plugin, slug, "", static_cast<int>(total));
        return 0;
    });
    http::on_pool(db, [&] {
        store.saved_read_rollup(plugin, slug, completed, 0);
        return 0;
    });
}
/** \brief Derive status + chapter totals/read counts from the plugin's own
 *  detail+chapters envelopes and persist them on the saved-manga row.
 *  Reader position POST / continue-read call `saved_clear_update` directly. */
inline void sync_saved_stats(Store& store, BS::thread_pool<>& db, const std::string& plugin, const std::string& slug,
                             const std::string& detail_env, const std::string& chapters_env) {
    // chapters envelope -> total from the list size (chapters-only path needs
    // it too: auto-update G1 advances total_ch + bit0 without a detail call)
    const auto chs = envelope_data(chapters_env);
    int total = 0;
    if (chs) {
        total = static_cast<int>(chs->size());
    }
    // detail envelope -> status + updated_at on save
    if (const auto d = detail::envelope_object(detail_env)) {
        const std::string status = detail::envelope_str(*d, "status");
        // library-dedup 1.2: synopsis (or 'description') + alt_titles from the SOURCE envelope
        std::string syn = detail::envelope_str(*d, "synopsis");
        if (syn.empty()) {
            syn = detail::envelope_str(*d, "description");
        }
        const std::string alts = detail::envelope_raw_json(*d, "alt_titles");
        http::on_pool(db, [&] {
            store.saved_stats_update(plugin, slug, status, chs ? total : -1);
            return 0;
        });
        http::on_pool(db, [&] {
            store.saved_signal_update(plugin, slug, syn, alts);
            return 0;
        });
        // library-dedup 2.3: re-cluster after the signals changed --
        // store-contention C1: coalesced, the scheduler tick pays once
        store.mark_dup_dirty();
    } else if (chs) {
        // chapters-only (design D4): same stat write with an empty status so
        // the source status + category bits stay untouched; an absent saved
        // row stays a quiet no-op inside the store call.
        http::on_pool(db, [&] {
            store.saved_stats_update(plugin, slug, "", total);
            return 0;
        });
    }
    apply_read_rollup(store, db, plugin, slug, chapters_env);
}

/** \brief Shared refresh body for the detail-page button and the auto-update
 *  scheduler (auto-update 3.1). Calls the plugin methods (detail+chapters by
 *  default), writes envelopes through the cache, and stamps the
 *  `update_attempts` tracker. Http-aware: the route maps the result enum to
 *  status codes; the scheduler uses the enum directly. */
inline std::string refresh_error_text(const std::string& method, const std::string& envelope) {
    // Prefer the plugin's own error string; fall back to the method name.
    glz::generic env;
    if (!glz::read_json(env, envelope) || !env.is_object()) {
        return "refresh failed: " + method;
    }
    auto* const it = env.get_object().find("error");
    if (it != env.get_object().end()) {
        if (const auto* s = std::get_if<std::string>(&it->second.data); (s != nullptr) && (!s->empty())) {
            return *s;
        }
    }
    return "refresh failed: " + method;
}

enum class RefreshResult : std::int8_t { kOk, kRejected, kDisabled, kOrphan, kUpstreamFailed };

struct RefreshOutcome {
    RefreshResult result = RefreshResult::kOk;
    std::string failed_method; // set when result == kUpstreamFailed
    std::string envelope;      // the failing envelope (for error text), else last chapters body
};

/** \brief `chapters_only`: skip the detail call (auto-update runs cheap
 *  freshness checks). The detail-page path passes false and refreshes both. */
inline RefreshOutcome refresh_title(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                    plugin::Host& plugins, const std::string& plugin, const std::string& slug,
                                    bool chapters_only = false) {
    RefreshOutcome out;
    if (!http::api::valid_title_pair(plugin, slug)) {
        out.result = RefreshResult::kRejected; // not an attempt: nothing upstream was hit
        return out;
    }
    const bool enabled = http::on_pool(db, [&] { return store.plugin_enabled(plugin); });
    if (!enabled) {
        out.result = RefreshResult::kDisabled; // not an attempt either
        return out;
    }
    // library-ownership 3.3: no registered plugin = no upstream to refresh
    // (the entry serves from local rows; the scheduler also skips orphans).
    if (http::on_pool(db, [&] { return store.plugin_upstream(plugin); }) == Upstream::orphan) {
        out.result = RefreshResult::kOrphan;
        return out;
    }
    crow::json::wvalue p;
    p["id"] = slug;
    const std::string params = p.dump();
    const std::vector<const char*> methods =
        chapters_only ? std::vector<const char*>{"chapters"} : std::vector<const char*>{"detail", "chapters"};
    std::string fresh[2];
    for (std::size_t i = 0; i < methods.size(); ++i) {
        fresh[i] = http::on_pool(plugin_pool, [&] { return plugins.call(plugin, methods[i], params); });
        glz::generic env;
        if (glz::read_json(env, fresh[i]) || !envelope_ok(env)) {
            out.result = RefreshResult::kUpstreamFailed;
            out.failed_method = methods[i];
            out.envelope = fresh[i];
            const std::string err = refresh_error_text(methods[i], fresh[i]);
            http::on_pool(db, [&] { // 3.2: stamp the failure so backoff/rotation sees it
                store.upsert_update_attempt(plugin, slug, "error", err);
                return 0;
            });
            return out;
        }
    }
    for (std::size_t i = 0; i < methods.size(); ++i) {
        const std::string key = stale_key(plugin, methods[i], params);
        const std::string body = fresh[i];
        http::on_pool(db, [&] {
            store.api_cache_put(plugin, key, params, body, static_cast<std::int64_t>(std::time(nullptr)), kApiCacheTtl);
            return 0;
        });
    }
    const std::string detail_env = chapters_only ? std::string{} : fresh[0];
    const std::string chapters_env = chapters_only ? fresh[0] : fresh[1];
    sync_saved_stats(store, db, plugin, slug, detail_env, chapters_env);
    if (!chapters_only) {
        // manga-detail-table D2: the refresh resolved a detail envelope -- write it
        // through like the route does (chosen overlay applied first; D2 keeps the
        // write out of apply_chosen itself).
        http::api::materialize_detail(store, db, plugin, slug,
                                      http::api::apply_chosen(store, db, plugin, slug, fresh[0]));
    }
    http::on_pool(db, [&] { // 3.2: stamp success (resets backoff)
        store.upsert_update_attempt(plugin, slug, "ok", "");
        return 0;
    });
    out.result = RefreshResult::kOk;
    out.envelope = chapters_env;
    return out;
}
