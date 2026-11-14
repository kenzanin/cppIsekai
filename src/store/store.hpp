#pragma once

/** \file store.hpp
 *  \brief The only SQL in the program lives in this class; callers get
 *  methods, never statement text (sqlite_orm vendored, Pimpl-hidden).
 *
 *  ponytail: sqlite_orm is not thread-safe; all calls must come from the db
 *  worker. */
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Plugin {
    std::int64_t id{};
    std::string name;
    int titles{};
    std::string plugin_id; // stable plugin key (folder id); empty for seed rows
    std::string version;
    bool enabled{true};
    std::string runtime; // lua/js/wasm; empty for legacy rows
    std::string caps;    // comma-separated capability list
};

struct Title {
    std::int64_t id{};
    std::string name;
    std::string plugin;
};

/** Last-good / fresh read model for plugin API responses: payload is JSONB
 *  binary (jsonb()/json()), one row per (plugin_id, query_hash). */
struct ApiCacheRow {
    std::string params_json; // canonical params, for debugging/inspection
    std::string payload;     // envelope JSON, decoded via json(payload)
    std::int64_t fetched_at{};
    std::int64_t ttl_s{};
};

/** One user-saved manga (library-save): denormalized name/cover so the grid
 *  never calls plugins at render; keyed unique on (plugin, slug). */
struct SavedManga {
    std::string plugin;
    std::string slug;
    std::string name;
    std::string cover;             // remote uploads URL as served by the plugin
    std::int64_t added_at{};       // unix seconds; stamped by the store on upsert
    std::string status{"ongoing"}; // plugin source envelope, verbatim; unknown values stored as-is
    int total_ch{};                // chapters reported by the source envelope; 0 = unknown
    int read_ch{};                 // chapters fully read at the last rollup
    int has_update{};              // 1 when a refresh observed growth beyond stored total_ch
    std::int64_t last_read_at{};   // reader position timestamp; 0 = never opened
    std::int64_t updated_at{};     // unix seconds of last envelope-derived change; 0 = never synced
    std::string synopsis{};        // plugin-source detail envelope synopsis (or 'description'; NOT enrichment)
    std::string alt_titles{};      // plugin-source detail envelope alt titles, JSON array verbatim
    int dup_group{};               // 0 = unique; recomputed from FTS5 + Dice similarity (library-dedup 2.2)
    int status_flags{};            // bit0=is_new, bit1=is_done; free bits for future status without migration
};

/** Last-read position of one manga (reader-page): resume point + continue
 *  hint. Keyed by (plugin, slug). */
struct ReaderPosition {
    std::string plugin;
    std::string slug;
    std::string chapter_id;
    double chapter_number{};   // denormalized: the hint shows it without a plugin call
    int page{1};               // 1-based
    std::string mode;          // reading mode; empty = unset
    std::string direction;     // ltr | rtl; empty = unset
    std::string zoom;          // fit-h | fit-w | 1:1; empty = unset
    std::int64_t updated_at{}; // unix seconds; stamped by the store on write
};

/** Auto-update attempt record: one row per (plugin, slug), written by both
 *  the scheduler and the manual refresh path. */
struct UpdateAttempt {
    std::string plugin;
    std::string slug;
    std::int64_t last_attempt_at{}; // unix seconds
    std::string outcome;            // ok|error|skipped
    std::string error;              // short error text; '' on success
    int consecutive_errors{};
    std::int64_t backoff_until{}; // 0 = not backed off
};

struct StoreImpl;

class Store {
public:
    explicit Store(const std::string& path);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    std::vector<Plugin> plugins();
    /** \brief Register/upsert a discovered plugin keyed by its stable id. */
    void upsert_plugin(const std::string& id, const std::string& name, const std::string& version,
                       const std::string& capabilities, const std::string& runtime);
    void set_plugin_enabled(std::int64_t id, bool on);
    /** \brief Gate for call paths; absent row (unregistered id) counts as
     *  enabled. */
    bool plugin_enabled(const std::string& plugin_id);
    /** \brief plugin-lifecycle 2.1: folder-removal GC -- every registry id
     *  absent from keep_ids loses its child traces (title via the registry
     *  row, plugin_state, reader_positions, saved_manga, title_meta,
     *  update_attempts, api_cache) and its plugin row, one transaction per
     *  plugin. Returns the removed ids. */
    std::vector<std::string> plugin_prune_absent(const std::vector<std::string>& keep_ids);
    int count(const std::string& plugin, const std::string& query);
    std::vector<Title> search(const std::string& plugin, const std::string& query, int limit, int offset);

    /** \brief Upsert one update_attempts row. Success (outcome=ok) resets
     *  consecutive_errors/backoff_until; errors increment the counter and set
     *  backoff_until = now + min(2^n, 72)h. */
    void upsert_update_attempt(const std::string& plugin, const std::string& slug, const std::string& outcome,
                               const std::string& error);
    /** \brief Auto-update batch pick: saved_manga read_ch <= 1, not backed
     *  off, per_plugin_limit rows per plugin (never-attempted first, then
     *  oldest last_attempt_at). */
    std::vector<UpdateAttempt> update_candidates(std::int64_t now, int per_plugin_limit);
    /** \brief Single attempt row; nullopt when never attempted. */
    std::optional<UpdateAttempt> update_attempt_get(const std::string& plugin, const std::string& slug);

    /** \brief Per-plugin key-value state (tokens etc.); survives restart.
     *  nullopt when absent. */
    std::optional<std::string> state_get(const std::string& plugin_id, const std::string& key);
    void state_set(const std::string& plugin_id, const std::string& key, const std::string& value);
    /** \brief Values '1' under \p key_prefix; returns the full keys. */
    std::vector<std::string> state_keys_value(const std::string& plugin_id, const std::string& key_prefix,
                                              const std::string& value);

    /** \brief Upsert the last-read position of one manga; updated_at is
     *  stamped here, any nonzero value in \p pos is ignored. */
    void position_set(const ReaderPosition& pos);
    /** \brief nullopt when the manga was never opened. */
    std::optional<ReaderPosition> position_get(const std::string& plugin, const std::string& slug);
    /** \brief Most recently updated position across all manga (the single-user
     *  "live anchor": skip-rerun resumes there). nullopt when none exists. */
    std::optional<ReaderPosition> position_last();

    /** \brief Read one api_cache row; nullopt when absent. ttl/fetched_at are
     *  returned raw — the caller decides fresh vs stale. */
    std::optional<ApiCacheRow> api_cache_get(const std::string& plugin_id, const std::string& query_hash);
    /** \brief Upsert an api_cache row; payload is stored as JSONB binary. */
    void api_cache_put(const std::string& plugin_id, const std::string& query_hash, const std::string& params_json,
                       const std::string& payload, std::int64_t fetched_at, std::int64_t ttl_s);

    /** \brief Envelope-derived stat update (status, total_ch, has_update, updated_at). Only changed fields bump
     * updated_at. */
    void saved_stats_update(const std::string& plugin, const std::string& slug, const std::string& status,
                            int total_ch);
    /** \brief Library-dedup 2.2: FTS5 candidate + Dice similarity clustering; writes dup_group for ALL saved rows in
     * one txn. No-op (all 0) when FTS5 unavailable or library tiny. */
    void recompute_dup_groups();
    /** \brief Envelope-derived dedup signals (library-dedup 1.2): synopsis (or 'description' fallback) + alt_titles
     * JSON array, verbatim. Empty args = leave untouched. */
    void saved_signal_update(const std::string& plugin, const std::string& slug, const std::string& synopsis,
                             const std::string& alt_titles_json);
    /** \brief Rollup of fully-read chapters + last-read timestamp (0 = leave untouched). */
    void saved_read_rollup(const std::string& plugin, const std::string& slug, int read_ch, std::int64_t last_read_at);
    /** \brief Reader touch: stamp last_read_at=now, clear has_update when set. */
    void saved_clear_update(const std::string& plugin, const std::string& slug);

    /** \brief Upsert/unsave one saved manga; the saved list is the Library
     *  grid. added_at is stamped here on save (insert refreshes it). */
    void save_manga(const SavedManga& m);
    void unsave_manga(const std::string& plugin, const std::string& slug);
    std::vector<SavedManga> saved_list(); // added_at desc
    std::optional<SavedManga> saved_get(const std::string& plugin, const std::string& slug);

    /** \brief One enrichment row for a saved manga; fetched/chosen are raw JSON text. */
    struct TitleMetaRow {
        std::string plugin;
        std::string slug;
        std::string fetched; // JSON array, one entry per source; never expires
        std::string chosen;  // JSON object {title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[]}
        std::int64_t fetched_at{}; // last fetch watermark (unix seconds)
    };

    /** \brief nullopt when the manga never fetched; corrupt JSON reads as nullopt too (2.3). */
    std::optional<TitleMetaRow> title_meta_get(const std::string& plugin, const std::string& slug);
    void title_meta_put_fetched(const std::string& plugin, const std::string& slug, const std::string& fetched_json,
                                std::int64_t fetched_at);
    void title_meta_put_chosen(const std::string& plugin, const std::string& slug, const std::string& chosen_json);
    void title_meta_reset_cover(const std::string& plugin, const std::string& slug);
    void title_meta_reset_detail(const std::string& plugin, const std::string& slug);

private:
    std::unique_ptr<StoreImpl> impl_;
};
