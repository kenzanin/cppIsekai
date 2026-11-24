// library-ownership: the ephemeral browse store. Pairs with no saved_manga row
// keep their state here; a save promotes it, a restart discards it. Every
// public entry is reached through the routed store methods — nothing outside
// src/store writes to these structures.

#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <utility>

using namespace sqlite_orm;


Upstream Store::plugin_upstream(const std::string& plugin_id) const {
    const store_detail::OpTrace _op{"plugin_upstream"};
    if (plugin_id.empty()) {
        return Upstream::live; // legacy seed rows carry no id to gate on
    }
    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(
        where(c(&store_detail::PluginEntity::plugin_id) == plugin_id));
    if (rows.empty()) {
        return Upstream::orphan;
    }
    return rows.front().enabled != 0 ? Upstream::live : Upstream::offline;
}

std::vector<std::pair<std::string, std::string>> Store::orphan_list() {
    const store_detail::OpTrace _op{"orphan_list"};
    sqlite3* db = nullptr;
    std::vector<std::pair<std::string, std::string>> out;
    if (sqlite3_open(impl_->path.c_str(), &db) != SQLITE_OK) {
        return out;
    }
    store_detail::attach_busy(db);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT plugin, slug FROM saved_manga s WHERE NOT EXISTS"
                      "(SELECT 1 FROM plugin p WHERE p.plugin_id = s.plugin) ORDER BY plugin, slug";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            out.emplace_back(p != nullptr ? p : "", s != nullptr ? s : "");
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return out;
}

void Store::pair_novel_set(const std::string& plugin, const std::string& slug, const bool on) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        if (!rows.empty()) {
            rows.front().novel = on ? 1 : 0;
            impl_->storage.update(rows.front());
        }
        return;
    }
    ephem_create(plugin, slug).novel = on;
}

std::optional<bool> Store::pair_novel_get(const std::string& plugin, const std::string& slug) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        if (rows.empty()) {
            return std::nullopt;
        }
        return rows.front().novel != 0;
    }
    if (const auto* e = ephem_find(plugin, slug); e != nullptr) {
        return e->novel;
    }
    return std::nullopt;
}

void Store::pair_detail_url_set(const std::string& plugin, const std::string& slug, const std::string& url) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        if (!rows.empty()) {
            rows.front().detail_url = url;
            impl_->storage.update(rows.front());
        }
        return;
    }
    ephem_create(plugin, slug).detail_url = url;
}

std::string Store::pair_detail_url_get(const std::string& plugin, const std::string& slug) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        return rows.empty() ? std::string{} : rows.front().detail_url;
    }
    if (const auto* e = ephem_find(plugin, slug); e != nullptr) {
        return e->detail_url;
    }
    return {};
}

void Store::pair_chapters_fp_set(const std::string& plugin, const std::string& slug, const std::string& fp) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        if (!rows.empty()) {
            rows.front().chapters_fp = fp;
            impl_->storage.update(rows.front());
        }
        return;
    }
    ephem_create(plugin, slug).chapters_fp = fp;
}

std::string Store::pair_chapters_fp_get(const std::string& plugin, const std::string& slug) {
    if (saved_exists(plugin, slug)) {
        auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
            where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                  c(&store_detail::SavedMangaEntity::slug) == slug));
        return rows.empty() ? std::string{} : rows.front().chapters_fp;
    }
    if (const auto* e = ephem_find(plugin, slug); e != nullptr) {
        return e->chapters_fp;
    }
    return {};
}
