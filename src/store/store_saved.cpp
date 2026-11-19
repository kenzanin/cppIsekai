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
    };
}

namespace {
// status-fold -> one-hot category mask (design D3): lowercase compare, unknown
// strings carry no category bit and are not an error.
unsigned status_category_mask(std::string_view status) {
    std::string low(status);
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
    if (low == "ongoing") {
        return static_cast<unsigned>(std::to_underlying(StatusFlag::kOngoing));
    }
    if (low == "hiatus") {
        return static_cast<unsigned>(std::to_underlying(StatusFlag::kHiatus));
    }
    if (low == "canceled" || low == "cancelled") {
        return static_cast<unsigned>(std::to_underlying(StatusFlag::kCanceled));
    }
    if (low == "completed") {
        return static_cast<unsigned>(std::to_underlying(StatusFlag::kCompleted));
    }
    return 0;
}
// Single composition for every stat writer (design D2): rebuild the bitfield
// from the entity + incoming change so the bits never drift apart. An empty
// incoming status means "leave the stored status alone" -> the category is
// derived from the stored status; the dim bit is owned by the toggle path and
// always survives.
int compose_flags(const store_detail::SavedMangaEntity& e, const std::string& status, bool is_new) {
    const unsigned cat = status_category_mask(status.empty() ? e.status : status);
    const bool done = e.total_ch > 0 && e.read_ch >= e.total_ch;
    const unsigned new_bit = (is_new && !done) ? static_cast<unsigned>(std::to_underlying(StatusFlag::kNew)) : 0u;
    const unsigned dim = static_cast<unsigned>(e.status_flags) & static_cast<unsigned>(std::to_underlying(StatusFlag::kDim));
    return static_cast<int>(cat | (done ? static_cast<unsigned>(std::to_underlying(StatusFlag::kDone)) : 0u) | new_bit | dim);
}
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
        // manga-detail-table: materialized display detail survives a re-save
        e.authors = rows[0].authors;
        e.artists = rows[0].artists;
        e.tags = rows[0].tags;
        e.synopsis_display = rows[0].synopsis_display;
    }
    impl_->storage.replace(e);
    store_detail::fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis); // library-dedup 1.4
}

void Store::saved_stats_update(const std::string& plugin, const std::string& slug, const std::string& status,
                               int total_ch) {
    const store_detail::OpTrace _op{"saved_stats_update"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    const int new_total = total_ch < 0 ? e.total_ch : total_ch; // <0 = unknown, keep stored
    const bool changed = (!status.empty() && e.status != status) || e.total_ch != new_total;
    if (!status.empty()) {
        e.status = status;
    }
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    // is_new = "previously set OR grew in this call" (design D4): a flat
    // refresh neither raises nor clears the badge.
    const bool grown = new_total > e.total_ch;
    const int news = (e.status_flags & static_cast<int>(std::to_underlying(StatusFlag::kNew))) | (grown ? 1 : 0);
    e.total_ch = new_total;            // compose against the NEW total (done = read_ch >= total_ch)
    e.status_flags = compose_flags(e, status, news != 0);
    if (changed) {
        e.updated_at = now;
    }
    // FTS NOT re-synced here (library-dedup 1.4): stats path touches no indexed column (name/alt_titles/synopsis).
    impl_->storage.replace(e);
}

void Store::saved_signal_update(const std::string& plugin, const std::string& slug, const std::string& synopsis,
                                const std::string& alt_titles_json) {
    const store_detail::OpTrace _op{"saved_signal_update"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    bool changed = false;
    if (!synopsis.empty() && e.synopsis != synopsis) {
        e.synopsis = synopsis;
        changed = true;
    }
    if (!alt_titles_json.empty() && e.alt_titles != alt_titles_json) {
        e.alt_titles = alt_titles_json;
        changed = true;
    }
    if (changed) {
        e.updated_at = static_cast<std::int64_t>(std::time(nullptr));
        impl_->storage.replace(e);
        store_detail::fts_sync(*impl_, e.plugin, e.slug, e.name, e.alt_titles, e.synopsis); // library-dedup 1.4
    }
}

void Store::saved_read_rollup(const std::string& plugin, const std::string& slug, int read_ch,
                              std::int64_t last_read_at) {
    const store_detail::OpTrace _op{"saved_read_rollup"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    e.read_ch = read_ch;
    if (last_read_at > 0) {
        e.last_read_at = last_read_at;
    }
    // same composition as the stats path (design D2): read_ch drives bit1,
    // the category + dim bits ride through untouched. is_new is "previously
    // set OR grew in this call" (the rollup never observes the chapter list).
    const int news = e.status_flags & static_cast<int>(std::to_underlying(StatusFlag::kNew));
    e.status_flags = compose_flags(e, "", news != 0);
    impl_->storage.replace(e);
}

void Store::saved_clear_update(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"saved_clear_update"};
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    auto& e = rows[0];
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    e.last_read_at = now; // reader touch (library-grid-stats 2.4), flag or not
    // detail is the source of truth for status_flags: a reader touch means the
    // title is no longer "new" — clear bit0 so library grid and detail agree;
    // category and dim bits survive.
    e.status_flags &= ~static_cast<int>(std::to_underlying(StatusFlag::kNew));
    impl_->storage.replace(e);
}

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
    impl_->storage.remove_all<store_detail::SavedMangaEntity>(where(
        c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
    store_detail::fts_delete(*impl_, plugin, slug); // library-dedup 1.4
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
        });
    }
    return out;
}
