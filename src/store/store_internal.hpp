#pragma once
/** \file store_internal.hpp
 *  \brief Storage entities + Impl plumbing shared by the store TUs.
 */
#include <sqlite_orm/sqlite_orm.h>
#include <sqlite3.h>
#include <memory>
#include <string>
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
    std::string plugin;     // composite PK with slug: one row per saved manga
    std::string slug;
    std::string name;
    std::string cover;
    std::int64_t added_at{};
};

struct ApiCacheEntity {
    std::string plugin_id;   // composite PK with query_hash
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
        make_table("saved_manga", make_column("plugin", &SavedMangaEntity::plugin), make_column("slug", &SavedMangaEntity::slug),
                   make_column("name", &SavedMangaEntity::name), make_column("cover", &SavedMangaEntity::cover),
                   make_column("added_at", &SavedMangaEntity::added_at),
                   primary_key(&SavedMangaEntity::plugin, &SavedMangaEntity::slug)),
        make_table("api_cache", make_column("plugin_id", &ApiCacheEntity::plugin_id),
                   make_column("query_hash", &ApiCacheEntity::query_hash),
                   make_column("params_json", &ApiCacheEntity::params_json),
                   make_column("fetched_at", &ApiCacheEntity::fetched_at),
                   make_column("ttl_s", &ApiCacheEntity::ttl_s),
                   primary_key(&ApiCacheEntity::plugin_id, &ApiCacheEntity::query_hash)));
}

using StorageType = decltype(create_storage(""));

} // namespace store_detail

struct StoreImpl {
    std::string path; // kept for the raw api_cache JSONB connection
    store_detail::StorageType storage;
    explicit StoreImpl(const std::string& p) : path(p), storage(store_detail::create_storage(p)) {}
};
