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

#include "reader/window.hpp"
#include "position_api.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace http {
namespace api {

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
inline constexpr const char* kDimBlurKey = "host/dim_blur";

// Typed readers for request/state JSON: absent or wrong-typed -> default.
// House crow accessors throw on a type mismatch; bodies are a trust boundary.
inline std::string str_field(const crow::json::rvalue& j, const char* key) {
    return j.has(key) && j[key].t() == crow::json::type::String ? std::string(j[key].s()) : std::string{};
}
inline long num_field(const crow::json::rvalue& j, const char* key, long def) {
    return j.has(key) && j[key].t() == crow::json::type::Number ? j[key].i() : def;
}

// manga-detail 1.1: the {plugin, slug} path pair guard. Slugs are our own
// cache-layout keys (fs-safe, case-preserving; plugin ids like atsumaru's H6P2Z carry uppercase); anything carrying separators or dot
// segments is not a slug we minted -> 404 before any plugin or fs touch
// (design D5). Returns false when the pair needs the not-found response.
inline bool valid_title_pair(const std::string& plugin, const std::string& slug) {
    const auto ok = [](const std::string& s) {
        if (s.empty() || s.size() > 128 ||
            s != s.substr(0, s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_."))) {
            return false;
        }
        if (s.find("..") != std::string::npos) {
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
                                       plugin::Host& plugins, const std::string& plugin_id,
                                       const std::string& manga_id, const reader::Position& pos);

// Route-group registration, one TU each (http_api_*.cpp).
void register_admin(Ctx&);   // plugin registry/admin + config endpoints
void register_library(Ctx&); // saved-library CRUD
void register_title(Ctx&);   // detail/related/chapters/refresh envelopes
void register_reader(Ctx&);  // reader window + position routes
void register_state(Ctx&);   // chapter skip/read/state flags
void register_content(Ctx&); // reader pages, image cache, cbz export
void register_enrich(Ctx&);  // enrichment fetch/chosen/reset

} // namespace api
} // namespace http

// --- saved-manga stats sync (library-grid-stats 2.x) ---
namespace detail {
inline std::optional<glz::generic::object_t> envelope_object(const std::string& env) {
    glz::generic parsed;
    if (glz::read_json(parsed, env) || !envelope_ok(parsed) || !parsed.is_object()) return std::nullopt;
    const auto data = parsed.get_object().find("data");
    if (data == parsed.get_object().end() || !data->second.is_object()) return std::nullopt;
    return data->second.get_object();
}
inline std::string envelope_str(const glz::generic::object_t& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end()) return {};
    if (const auto* s = std::get_if<std::string>(&it->second.data)) return *s;
    return {};
}
// library-dedup 1.2: alt_titles re-serialized verbatim (source envelope only)
inline std::string envelope_raw_json(const glz::generic::object_t& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end() || it->second.is_null()) return {};
    return glz::write_json(it->second).value_or(std::string{});
}
} // namespace detail

/** \brief Derive status + chapter totals/read counts from the plugin's own
 *  detail+chapters envelopes and persist them on the saved-manga row.
 *  Reader position POST / continue-read call `saved_clear_update` directly. */
inline void sync_saved_stats(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                              const std::string& slug, const std::string& detail_env,
                              const std::string& chapters_env) {
    // detail envelope -> status + updated_at on save
    if (const auto d = detail::envelope_object(detail_env)) {
        const std::string status = detail::envelope_str(*d, "status");
        int total = 0;
        if (const auto chs = envelope_data(chapters_env)) total = static_cast<int>(chs->size());
        // library-dedup 1.2: synopsis (or 'description') + alt_titles from the SOURCE envelope
        std::string syn = detail::envelope_str(*d, "synopsis");
        if (syn.empty()) syn = detail::envelope_str(*d, "description");
        const std::string alts = detail::envelope_raw_json(*d, "alt_titles");
        http::on_pool(db, [&] { store.saved_stats_update(plugin, slug, status, total); return 0; });
        http::on_pool(db, [&] { store.saved_signal_update(plugin, slug, syn, alts); return 0; });
        // library-dedup 2.3: re-cluster after the signals changed
        http::on_pool(db, [&] { store.recompute_dup_groups(); return 0; });
    }
    // chapters envelope -> total_ch + read_ch rollup
    const auto chs = envelope_data(chapters_env);
    if (!chs) return;
    int completed = 0;
    for (const auto& item : *chs) {
        if (!item.is_object()) continue;
        const auto& entry = item.get_object();
        const auto id_it = entry.find("id");
        if (id_it == entry.end()) continue;
        const auto* id_s = std::get_if<std::string>(&id_it->second.data);
        if (!id_s) continue;
        int pages = 0;
        if (const auto p = entry.find("pages"); p != entry.end())
            if (const auto* d = std::get_if<double>(&p->second.data)) pages = static_cast<int>(*d);
        const auto stored = http::on_pool(db, [&] { return store.state_get(http::api::kStateHost, std::string(http::api::kReadPrefix) + *id_s); });
        if (!stored || stored->empty()) continue;
        const int read = std::atoi(stored->c_str());
        if (pages > 0 ? read >= pages : read > 0) ++completed;
    }
    http::on_pool(db, [&] { store.saved_read_rollup(plugin, slug, completed, 0); return 0; });
}
