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
#include <string>
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

// Reserved plugin_state rows for host bookkeeping: per-chapter skip/read
// flags and the last reader position (the anchor the skip-rerun reads back).
inline constexpr const char* kStateHost = "host";
inline constexpr const char* kSkipPrefix = "host/skip/";
inline constexpr const char* kReadPrefix = "host/read/";
inline constexpr const char* kPositionKey = "host/pos";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.
inline std::string str_field(const crow::json::rvalue& j, const char* key) {
    return j.has(key) && j[key].t() == crow::json::type::String ? std::string(j[key].s()) : std::string{};
}
inline long num_field(const crow::json::rvalue& j, const char* key, long def) {
    return j.has(key) && j[key].t() == crow::json::type::Number ? j[key].i() : def;
}

// manga-detail 1.1: the {plugin, slug} path pair guard. Slugs are our own
// cache-layout keys (fs-safe, case-preserving; plugin ids like atsumaru's H6P2Z carry uppercase); anything carrying
// separators or dot segments is not a slug we minted -> 404 before any plugin or fs touch (design D5). Returns false
// when the pair needs the not-found response.
inline bool valid_title_pair(const std::string& plugin, const std::string& slug) {
    const auto ok = [](const std::string& s) {
        if (s.empty() || s.size() > 128 ||
            s !=
                s.substr(0, s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_."))) {
            return false;
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
// math in reader::next_window. Empty result on any plugin failure -- the
// reader merely loses its prefetch. The worker (task 3.3) mirrors this shape.
// Defined in http_api_reader.cpp; the state TU reuses it for the skip-rerun.
std::vector<std::string> reader_window(Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                                       plugin::Host& plugins, const std::string& plugin_id, const std::string& manga_id,
                                       const reader::Position& pos);

// Route-group registration, one TU each (http_api_*.cpp).
void register_admin(Ctx& /*c*/); // plugin registry/admin + config endpoints
void register_library(Ctx& /*c*/); // saved-library CRUD
void register_title(Ctx& /*c*/);   // detail/related/chapters/refresh envelopes
void register_reader(Ctx& /*c*/); // reader window + position routes
void register_state(Ctx& /*c*/); // chapter skip/read/state flags
void register_content(Ctx& /*c*/); // reader pages, image cache, cbz export
void register_enrich(Ctx& /*c*/); // enrichment fetch/chosen/reset

} // namespace http::api

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
            store.saved_stats_update(plugin, slug, status, total);
            return 0;
        });
        http::on_pool(db, [&] {
            store.saved_signal_update(plugin, slug, syn, alts);
            return 0;
        });
        // library-dedup 2.3: re-cluster after the signals changed
        http::on_pool(db, [&] {
            store.recompute_dup_groups();
            return 0;
        });
    } else if (chs) {
        // chapters-only (design D4): same stat write with an empty status so
        // the source status + category bits stay untouched; an absent saved
        // row stays a quiet no-op inside the store call.
        http::on_pool(db, [&] {
            store.saved_stats_update(plugin, slug, "", total);
            return 0;
        });
    }
    // chapters envelope -> total_ch + read_ch rollup
    if (!chs) {
        return;
    }
    int completed = 0;
    for (const auto& item : *chs) {
        if (!item.is_object()) {
            continue;
        }
        const auto& entry = item.get_object();
        const auto* const id_it = entry.find("id");
        if (id_it == entry.end()) {
            continue;
        }
        const auto* id_s = std::get_if<std::string>(&id_it->second.data);
        if (id_s == nullptr) {
            continue;
        }
        int pages = 0;
        if (const auto* const p = entry.find("pages"); p != entry.end()) {
            if (const auto* d = std::get_if<double>(&p->second.data)) {
                pages = static_cast<int>(*d);
            }
        }
        const auto stored = http::on_pool(
            db, [&] { return store.state_get(http::api::kStateHost, std::string(http::api::kReadPrefix) + *id_s); });
        if (!stored || stored->empty()) {
            continue;
        }
        const int read = std::atoi(stored->c_str());
        if (pages > 0 ? read >= pages : read > 0) {
            ++completed;
        }
    }
    http::on_pool(db, [&] {
        store.saved_read_rollup(plugin, slug, completed, 0);
        return 0;
    });
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

enum class RefreshResult { kOk, kRejected, kDisabled, kUpstreamFailed };

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
    http::on_pool(db, [&] { // 3.2: stamp success (resets backoff)
        store.upsert_update_attempt(plugin, slug, "ok", "");
        return 0;
    });
    out.result = RefreshResult::kOk;
    out.envelope = chapters_env;
    return out;
}
