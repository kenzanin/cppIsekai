#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace sqlite_orm;

bool Store::saved_detail_put(const SavedDetailPut& in) {
    const store_detail::OpTrace _op{"saved_detail_put"};
    const auto& plugin = in.plugin;
    const auto& slug = in.slug;
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return false;
    }
    auto& e = rows[0];
    // name guard mirrors D9: never land a placeholder over a real name
    std::string new_name = e.name;
    if (!in.name.empty() && in.name != slug) {
        new_name = in.name;
    }
    const bool name_changed = new_name != e.name;
    const bool changed = name_changed || e.authors != in.authors || e.artists != in.artists ||
                         e.tags != in.tags || e.synopsis_display != in.synopsis_display ||
                         (!in.cover.empty() && e.cover != in.cover);
    if (!changed) {
        return false; // plain page open stays read-only (design D2)
    }
    e.name = new_name;
    e.authors = in.authors;
    e.artists = in.artists;
    e.tags = in.tags;
    e.synopsis_display = in.synopsis_display;
    if (!in.cover.empty()) {
        e.cover = in.cover; // refresh keeps the cover current (sites rotate/prune upload urls)
    }
    impl_->storage.replace(e);
    if (name_changed) {
        // name is an FTS-indexed column (library-dedup 1.4); signals above are untouched (D4)
        store_detail::fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis);
    }
    return true;
}

void Store::saved_detail_clear(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"saved_detail_clear"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    if (e.authors.empty() && e.artists.empty() && e.tags.empty() && e.synopsis_display.empty()) {
        return;
    }
    e.authors.clear();
    e.artists.clear();
    e.tags.clear();
    e.synopsis_display.clear();
    impl_->storage.replace(e);
}

void Store::saved_dim_update(const std::string& plugin, const std::string& slug, bool on) {
    const store_detail::OpTrace _op{"saved_dim_update"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return; // unsaved manga has no row to hold the bit: toggle is a no-op
    }
    auto& e = rows[0];
    const int dim = static_cast<int>(std::to_underlying(StatusFlag::kDim));
    e.status_flags = on ? e.status_flags | dim : e.status_flags & ~dim; // bit6 only, owned by this path
    impl_->storage.replace(e);
}

std::vector<std::pair<std::string, std::string>> Store::saved_dim_list() {
    const store_detail::OpTrace _op{"saved_dim_list"};
    const int dim = static_cast<int>(std::to_underlying(StatusFlag::kDim));
    auto rows = impl_->storage.select(
        columns(&store_detail::SavedMangaEntity::plugin, &store_detail::SavedMangaEntity::slug),
        where(c(&store_detail::SavedMangaEntity::status_flags) & dim));
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.emplace_back(std::get<0>(r), std::get<1>(r));
    }
    return out;
}

void Store::unsave_manga(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"unsave_manga"};
    // library-promote-on-save 4.1: unsave = full wipe. One txn takes the row
    // and every pair-keyed satellite (chapters/pages/position/attempts/title_meta
    // + the pair-scoped host keys); the api_cache rows go on a raw connection
    // after commit (JSONB/JSON1, same pattern as api_cache_get). Re-add hence
    // starts exactly like a fresh title from search: empty row, cold cache.
    auto& st = impl_->storage;
    store_detail::TxGuard tx{st};
    st.begin_transaction();
    ephem_erase(plugin, slug); // browse state dies with the library entry
    st.remove_all<store_detail::SavedMangaEntity>(
        where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
              c(&store_detail::SavedMangaEntity::slug) == slug));
    st.remove_all<store_detail::ChapterEntity>(
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug));
    st.remove_all<store_detail::ChapterPageEntity>(
        where(c(&store_detail::ChapterPageEntity::plugin) == plugin &&
              c(&store_detail::ChapterPageEntity::slug) == slug));
    st.remove_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == plugin &&
              c(&store_detail::ReaderPositionEntity::slug) == slug));
    st.remove_all<store_detail::UpdateAttemptsEntity>(
        where(c(&store_detail::UpdateAttemptsEntity::plugin) == plugin &&
              c(&store_detail::UpdateAttemptsEntity::slug) == slug));
    st.remove_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin &&
              c(&store_detail::TitleMetaEntity::slug) == slug));
    // pair-scoped host bookkeeping only; the legacy host/skip|read|total keys
    // are chapter_id-keyed (no slug) and ride the D2 inheritance by design.
    const std::string novel = std::string(kStateHost) + "/novel/" + plugin + "/" + slug;
    const std::string detail_url = std::string(kStateHost) + "/detail_url/" + plugin + "/" + slug;
    const std::string fp = std::string(kChaptersFpPrefix) + plugin + "/" + slug;
    st.remove_all<store_detail::StateEntity>(where(c(&store_detail::StateEntity::plugin_id) == kStateHost &&
                                                  (c(&store_detail::StateEntity::key) == novel ||
                                                   c(&store_detail::StateEntity::key) == detail_url ||
                                                   c(&store_detail::StateEntity::key) == fp)));
    tx.commit();
    store_detail::fts_delete(*impl_, plugin, slug); // library-dedup 1.4
    // envelope cache: exact id + the chapter-scoped "<slug>:..." page keys
    sqlite3* raw = nullptr;
    if (sqlite3_open(impl_->path.c_str(), &raw) == SQLITE_OK) {
        store_detail::attach_busy(raw);
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "DELETE FROM api_cache WHERE plugin_id=? AND (json_extract(params_json,'$.id')=? OR "
            "json_extract(params_json,'$.id') LIKE ?||':%')";
        if (sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr) == SQLITE_OK &&
            sqlite3_bind_text(stmt, 1, plugin.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_text(stmt, 2, slug.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_text(stmt, 3, slug.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK) {
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(raw);
    }
}

std::vector<SavedManga> Store::saved_list() {
    const store_detail::OpTrace _op{"saved_list"};
    // added_at has second resolution, and REPLACE re-stamps it on re-save, so
    // newest-first by added_at alone is stable enough for the single-user grid.
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
        order_by(&store_detail::SavedMangaEntity::added_at).desc());
    std::vector<SavedManga> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(SavedManga{
            .plugin = r.plugin,
            .slug = r.slug,
            .name = r.name,
            .cover = r.cover,
            .added_at = r.added_at,
            .status = r.status,
            .total_ch = r.total_ch,
            .read_ch = r.read_ch,
            .last_read_at = r.last_read_at,
            .updated_at = r.updated_at,
            .synopsis = r.synopsis,
            .alt_titles = r.alt_titles,
            .dup_group = r.dup_group,
            .status_flags = r.status_flags,
            .authors = r.authors,
            .artists = r.artists,
            .tags = r.tags,
            .synopsis_display = r.synopsis_display,
            .pinned = r.pinned,
        });
    }
    return out;
}
