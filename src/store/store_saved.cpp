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

static store_detail::SavedMangaEntity saved_row(const SavedManga& m, std::int64_t added_at) {
    return store_detail::SavedMangaEntity{
        .plugin = m.plugin,
        .slug = m.slug,
        .name = m.name,
        .cover = m.cover,
        .added_at = added_at,
        .status = m.status.empty() ? std::string{"ongoing"} : m.status,
        .total_ch = m.total_ch,
        .read_ch = m.read_ch,
        .last_read_at = m.last_read_at,
        .updated_at = m.updated_at,
        .synopsis = m.synopsis,
        .alt_titles = m.alt_titles,
        .dup_group = m.dup_group,
        .status_flags = m.status_flags,
        .authors = m.authors,
        .artists = m.artists,
        .tags = m.tags,
        .synopsis_display = m.synopsis_display,
        .pinned = m.pinned,
        .chapters_fp = {},
        .novel = 0,
        .detail_url = {},
    };
}

namespace {
// Single composition for every stat writer (design D2): the bitfield helpers
// live in store_internal.hpp (shared with the library-migrate path).
using store_detail::compose_flags;
} // namespace

void Store::save_manga(const SavedManga& m) {
    const store_detail::OpTrace _op{"save_manga"};
    // REPLACE on the (plugin, slug) PK: refreshes name/cover/added_at on re-save.
    // Preserve any existing stat columns so a plain re-save never resets progress.
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == m.plugin && c(&store_detail::SavedMangaEntity::slug) == m.slug));
    store_detail::SavedMangaEntity e = saved_row(m, static_cast<std::int64_t>(std::time(nullptr)));
    if (!rows.empty()) {
        e.status = rows[0].status;
        e.total_ch = rows[0].total_ch;
        e.read_ch = rows[0].read_ch;
        e.last_read_at = rows[0].last_read_at;
        e.updated_at = rows[0].updated_at;
        // library-dedup 1.2: preserve signal columns (envelope sync owns them)
        e.synopsis = rows[0].synopsis;
        e.alt_titles = rows[0].alt_titles;
        e.dup_group = rows[0].dup_group;
        e.status_flags = rows[0].status_flags;
        e.authors = rows[0].authors;
        e.pinned = rows[0].pinned; // library-pin: re-save never unpins
        e.artists = rows[0].artists;
        e.tags = rows[0].tags;
        e.synopsis_display = rows[0].synopsis_display;
    }
    impl_->storage.replace(e);
    store_detail::fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis); // library-dedup 1.4
    // library-ownership 2.1: the row exists now -- promote any browse state
    // (flags/read_max/pages/position/enrichment) accumulated before the save.
    ephem_promote(m.plugin, m.slug);
}

