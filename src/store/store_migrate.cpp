// library-migrate: repoint a saved entry at another source (goIsekai
// MigrateMangaSource port). The read state carries by number boundary -- every
// target chapter with number <= the highest read source chapter becomes read
// (sub-chapters 6.1 included when 6 was the bound); per-chapter reader
// positions and cached page files do not carry (the UI warns about both).
#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <algorithm>
#include <ctime>
#include <string>

using namespace sqlite_orm;

bool Store::migrate_source(const std::string& old_plugin, const std::string& old_slug,
                           const std::string& new_plugin, const std::string& new_slug,
                           const MigrateTarget& to) {
    const store_detail::OpTrace _op{"migrate_source"};
    if (old_plugin.empty() || old_slug.empty() || new_plugin.empty() || new_slug.empty() ||
        (old_plugin == new_plugin && old_slug == new_slug)) {
        return false;
    }
    auto& st = impl_->storage;
    const auto pair_of = [](const std::string& p, const std::string& s) {
        return c(&store_detail::SavedMangaEntity::plugin) == p && c(&store_detail::SavedMangaEntity::slug) == s;
    };
    const auto ch_pair_of = [](const std::string& p, const std::string& s) {
        return c(&store_detail::ChapterEntity::plugin) == p && c(&store_detail::ChapterEntity::slug) == s;
    };

    // Reads before the write txn: the deferred-snapshot upgrade surfaces as an
    // instant "database is locked" otherwise (BUSY_SNAPSHOT, #5614). One
    // OpTrace holds the mutex for the whole method, so nothing slips in between.
    auto rows = st.get_all<store_detail::SavedMangaEntity>(where(pair_of(old_plugin, old_slug)));
    if (rows.empty()) {
        return false; // source not in the library
    }
    if (!st.get_all<store_detail::SavedMangaEntity>(where(pair_of(new_plugin, new_slug))).empty()) {
        return false; // target already saved
    }
    // library-ownership 2.1: a browsed-but-unsaved target holds its rows in the
    // ephemeral store -- stage them so the boundary carry sees them. The pair
    // becomes saved within this call's txn; the entry is discarded on success.
    // ponytail: a failure after staging leaves the staged rows until the next
    // save/reparse reclaims them.
    const bool staged = !saved_exists(new_plugin, new_slug);
    if (staged) {
        ephem_copy_into(new_plugin, new_slug);
    }
    // Read boundary: highest read chapter number on the old pair.
    double bound = -1.0;
    for (const auto& ch : st.get_all<store_detail::ChapterEntity>(where(ch_pair_of(old_plugin, old_slug)))) {
        if ((ch.flags & static_cast<int>(std::to_underlying(ChapterFlag::kRead))) != 0) {
            bound = std::max(bound, ch.number);
        }
    }
    // Target rows materialized by chapters_upsert before this call; flags may
    // already carry bits (a previewed target keeps its reads -- OR, not stomp).
    auto target_chs = st.get_all<store_detail::ChapterEntity>(where(ch_pair_of(new_plugin, new_slug)));
    std::optional<store_detail::TitleMetaEntity> meta;
    auto metas = st.get_all<store_detail::TitleMetaEntity>(where(
        c(&store_detail::TitleMetaEntity::plugin) == old_plugin &&
        c(&store_detail::TitleMetaEntity::slug) == old_slug));
    if (!metas.empty()) {
        meta = std::move(metas.front());
    }

    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    const int read_bit = static_cast<int>(std::to_underlying(ChapterFlag::kRead));
    int read_ch = 0;
    for (auto& ch : target_chs) {
        // boundary carry: OR, never stomp -- a previewed target keeps its reads
        if (bound >= 0.0 && ch.number <= bound && (ch.flags & read_bit) == 0) {
            ch.flags |= read_bit;
            ch.updated_at = now;
        }
        if ((ch.flags & read_bit) != 0) {
            ++read_ch;
        }
    }

    auto& e = rows.front();
    e.plugin = new_plugin;
    e.slug = new_slug;
    if (!to.name.empty()) {
        e.name = to.name;
    }
    if (!to.cover.empty()) {
        e.cover = to.cover;
    }
    if (!to.status.empty()) {
        e.status = to.status;
    }
    e.total_ch = to.total_ch;
    e.read_ch = read_ch;
    e.updated_at = now; // envelope-derived change stamp; last_read_at rides (work-level, not per-source)
    e.status_flags = store_detail::compose_flags(e, e.status, false);

    store_detail::TxGuard tx{st};
    st.begin_transaction();
    // PK rekey = remove+reinsert on the composite key; every other column rides.
    st.remove_all<store_detail::SavedMangaEntity>(where(pair_of(old_plugin, old_slug)));
    st.replace(e);
    st.remove_all<store_detail::ChapterEntity>(where(ch_pair_of(old_plugin, old_slug)));
    st.remove_all<store_detail::ChapterPageEntity>(
        where(c(&store_detail::ChapterPageEntity::plugin) == old_plugin &&
              c(&store_detail::ChapterPageEntity::slug) == old_slug));
    st.remove_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == old_plugin &&
              c(&store_detail::ReaderPositionEntity::slug) == old_slug));
    st.remove_all<store_detail::UpdateAttemptsEntity>(
        where(c(&store_detail::UpdateAttemptsEntity::plugin) == old_plugin &&
              c(&store_detail::UpdateAttemptsEntity::slug) == old_slug));
    // title_meta: the entry's fetched/chosen state IS the work's, move it; a
    // target preview row (never chosen from) yields.
    st.remove_all<store_detail::TitleMetaEntity>(where(
        c(&store_detail::TitleMetaEntity::plugin) == new_plugin &&
        c(&store_detail::TitleMetaEntity::slug) == new_slug));
    st.remove_all<store_detail::TitleMetaEntity>(where(
        c(&store_detail::TitleMetaEntity::plugin) == old_plugin &&
        c(&store_detail::TitleMetaEntity::slug) == old_slug));
    if (meta) {
        meta->plugin = new_plugin;
        meta->slug = new_slug;
        st.replace(*meta);
    }
    for (const auto& ch : target_chs) {
        if (bound >= 0.0 && ch.number <= bound) {
            st.update(ch);
        }
    }
    tx.commit();

    if (staged) {
        ephem_erase(new_plugin, new_slug);
    }
    // FTS on its own connection, post-commit (the helpers open a second
    // handle); a crash here self-heals on the next saved write.
    store_detail::fts_delete(*impl_, old_plugin, old_slug);
    store_detail::fts_sync(*impl_, new_plugin, new_slug, e.name, e.alt_titles, e.synopsis);
    return true;
}
