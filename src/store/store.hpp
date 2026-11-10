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
    std::string cover;       // remote uploads URL as served by the plugin
    std::int64_t added_at{}; // unix seconds; stamped by the store on upsert
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
    int count(const std::string& plugin, const std::string& query);
    std::vector<Title> search(const std::string& plugin, const std::string& query, int limit, int offset);

    /** \brief Per-plugin key-value state (tokens etc.); survives restart.
     *  nullopt when absent. */
    std::optional<std::string> state_get(const std::string& plugin_id, const std::string& key);
    void state_set(const std::string& plugin_id, const std::string& key, const std::string& value);

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
    void api_cache_put(const std::string& plugin_id, const std::string& query_hash,
                       const std::string& params_json, const std::string& payload,
                       std::int64_t fetched_at, std::int64_t ttl_s);

    /** \brief Upsert/unsave one saved manga; the saved list is the Library
     *  grid. added_at is stamped here on save (insert refreshes it). */
    void save_manga(const SavedManga& m);
    void unsave_manga(const std::string& plugin, const std::string& slug);
    std::vector<SavedManga> saved_list(); // added_at desc
    std::optional<SavedManga> saved_get(const std::string& plugin, const std::string& slug);

private:
    std::unique_ptr<StoreImpl> impl_;
};
