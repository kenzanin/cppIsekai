#pragma once
/** \file store_internal.hpp
 *  \brief Storage entities + Impl plumbing shared by the store TUs.
 */
#include <memory>
#include <sqlite3.h>
#include <string>
#include <sqlite_orm/sqlite_orm.h>
namespace store_detail {
using namespace sqlite_orm;

struct PluginEntity {
    std::int64_t id = 0;
    std::string name;
    std::string plugin_id; // stable plugin key; '' for the seed rows
    std::string version;
    std::string capabilities;
    int enabled = 1;
    std::string runtime;
};

struct TitleEntity {
    std::int64_t id = 0;
    std::int64_t plugin_id = 0;
    std::string name;
};

struct StateEntity {
    std::string plugin_id; // plugin folder id, not plugin.id FK
    std::string key;
    std::string value;
};

struct ReaderPositionEntity {
    std::string plugin; // composite PK with slug: one row per manga
    std::string slug;
    std::string chapter_id;
    double chapter_number{};
    int page{};
    std::string mode;
    std::string direction;
    std::string zoom;
    std::int64_t updated_at{};
};

struct SavedMangaEntity {
    std::string plugin; // composite PK with slug: one row per saved manga
    std::string slug;
    std::string name;
    std::string cover;
    std::int64_t added_at{};
    std::string status;          // ongoing|hiatus|complete|canceled (plugin source; stored verbatim)
    int total_ch{};              // chapters reported by the source envelope; 0 = unknown
    int read_ch{};               // chapters fully read (host/read >= pages) at last rollup
    std::int64_t last_read_at{}; // reader position POST timestamp; 0 = never opened
    std::int64_t updated_at{};   // unix seconds of last envelope-derived change; 0 = never synced
    std::string synopsis;        // plugin-source detail envelope synopsis (NOT enrichment)
    std::string alt_titles;      // plugin-source detail envelope alt titles, JSON array verbatim
    int dup_group{};             // 0 = unique; elsewhere groups same work (recomputed, never trusted across boots)
    int status_flags{};          // bit0=1 is_new, bit1=2 is_done, bit2=4 ongoing, bit3=8 hiatus,
                                 // bit4=16 canceled, bit5=32 completed, bit6=64 is_dim
};

struct TitleMetaEntity {
    std::string plugin; // composite PK with slug: one row per saved manga
    std::string slug;
    std::string fetched;       // JSON array: [{source, fetched_at, data}]; never expires
    std::string chosen;        // JSON object: {title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[]}
    std::int64_t fetched_at{}; // last fetch watermark (unix seconds)
};

struct UpdateAttemptsEntity {
    std::string plugin; // composite PK with slug: one row per manga
    std::string slug;
    std::int64_t last_attempt_at{}; // unix seconds
    std::string outcome;            // ok|error|skipped
    std::string error;              // short plugin/host error text; '' on success
    int consecutive_errors{};       // backoff curve input
    std::int64_t backoff_until{};   // 0 = no backoff
};

struct ApiCacheEntity {
    std::string plugin_id; // composite PK with query_hash
    std::string query_hash;
    std::string params_json;
    std::int64_t fetched_at{};
    std::int64_t ttl_s{};
    // payload deliberately NOT mapped here: sqlite_orm has no JSONB column;
    // the JSONB blob is written/read through the store's raw connection.
};

inline auto create_storage(const std::string& path) {
    return make_storage(
        path,
        make_table("plugin", make_column("id", &PluginEntity::id, primary_key().autoincrement()),
                   make_column("name", &PluginEntity::name, unique()),
                   make_column("plugin_id", &PluginEntity::plugin_id), make_column("version", &PluginEntity::version),
                   make_column("capabilities", &PluginEntity::capabilities),
                   make_column("enabled", &PluginEntity::enabled, default_value(1)),
                   make_column("runtime", &PluginEntity::runtime)),
        make_table("title", make_column("id", &TitleEntity::id, primary_key().autoincrement()),
                   make_column("plugin_id", &TitleEntity::plugin_id), make_column("name", &TitleEntity::name),
                   foreign_key(&TitleEntity::plugin_id).references(&PluginEntity::id)),
        make_table("plugin_state", make_column("plugin_id", &StateEntity::plugin_id),
                   make_column("key", &StateEntity::key), make_column("value", &StateEntity::value),
                   primary_key(&StateEntity::plugin_id, &StateEntity::key)),
        make_table("reader_positions", make_column("plugin", &ReaderPositionEntity::plugin),
                   make_column("slug", &ReaderPositionEntity::slug),
                   make_column("chapter_id", &ReaderPositionEntity::chapter_id),
                   make_column("chapter_number", &ReaderPositionEntity::chapter_number),
                   make_column("page", &ReaderPositionEntity::page), make_column("mode", &ReaderPositionEntity::mode),
                   make_column("direction", &ReaderPositionEntity::direction),
                   make_column("zoom", &ReaderPositionEntity::zoom, default_value("")),
                   make_column("updated_at", &ReaderPositionEntity::updated_at),
                   primary_key(&ReaderPositionEntity::plugin, &ReaderPositionEntity::slug)),
        make_table("saved_manga", make_column("plugin", &SavedMangaEntity::plugin),
                   make_column("slug", &SavedMangaEntity::slug), make_column("name", &SavedMangaEntity::name),
                   make_column("cover", &SavedMangaEntity::cover), make_column("added_at", &SavedMangaEntity::added_at),
                   make_column("status", &SavedMangaEntity::status, default_value("ongoing")),
                   make_column("total_ch", &SavedMangaEntity::total_ch, default_value(0)),
                   make_column("read_ch", &SavedMangaEntity::read_ch, default_value(0)),
                   make_column("last_read_at", &SavedMangaEntity::last_read_at, default_value(0)),
                   make_column("updated_at", &SavedMangaEntity::updated_at, default_value(0)),
                   make_column("synopsis", &SavedMangaEntity::synopsis, default_value("")),
                   make_column("alt_titles", &SavedMangaEntity::alt_titles, default_value("")),
                   make_column("dup_group", &SavedMangaEntity::dup_group, default_value(0)),
                  make_column("status_flags", &SavedMangaEntity::status_flags, default_value(0)),
                   primary_key(&SavedMangaEntity::plugin, &SavedMangaEntity::slug)),
        make_table("title_meta", make_column("plugin", &TitleMetaEntity::plugin),
                   make_column("slug", &TitleMetaEntity::slug), make_column("fetched", &TitleMetaEntity::fetched),
                   make_column("chosen", &TitleMetaEntity::chosen),
                   make_column("fetched_at", &TitleMetaEntity::fetched_at),
                   primary_key(&TitleMetaEntity::plugin, &TitleMetaEntity::slug)),
        make_table("update_attempts", make_column("plugin", &UpdateAttemptsEntity::plugin),
                   make_column("slug", &UpdateAttemptsEntity::slug),
                   make_column("last_attempt_at", &UpdateAttemptsEntity::last_attempt_at, default_value(0)),
                   make_column("outcome", &UpdateAttemptsEntity::outcome, default_value("")),
                   make_column("error", &UpdateAttemptsEntity::error, default_value("")),
                   make_column("consecutive_errors", &UpdateAttemptsEntity::consecutive_errors, default_value(0)),
                   make_column("backoff_until", &UpdateAttemptsEntity::backoff_until, default_value(0)),
                   primary_key(&UpdateAttemptsEntity::plugin, &UpdateAttemptsEntity::slug)),
        make_table("api_cache", make_column("plugin_id", &ApiCacheEntity::plugin_id),
                   make_column("query_hash", &ApiCacheEntity::query_hash),
                   make_column("params_json", &ApiCacheEntity::params_json),
                   make_column("fetched_at", &ApiCacheEntity::fetched_at), make_column("ttl_s", &ApiCacheEntity::ttl_s),
                   primary_key(&ApiCacheEntity::plugin_id, &ApiCacheEntity::query_hash)));
}

using StorageType = decltype(create_storage(""));

} // namespace store_detail

struct StoreImpl {
    std::string path; // kept for the raw api_cache JSONB connection
    store_detail::StorageType storage;
    bool fts5_ok = false; // library-dedup 1.3: false when the amalgamation lacks FTS5 (dedup disabled)
    explicit StoreImpl(const std::string& p) : path(p), storage(store_detail::create_storage(p)) {}
};
