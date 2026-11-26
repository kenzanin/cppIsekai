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
    const int old_flags = e.status_flags;
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
    // A row-served detail open runs this on every request, so write only when
    // the row actually moved -- an unconditional replace cost a WAL frame per
    // open for nothing.
    if (changed || e.status_flags != old_flags) {
        impl_->storage.replace(e);
    }
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
    const int old_read = e.read_ch;
    const int old_flags = e.status_flags;
    const std::int64_t old_last = e.last_read_at;
    e.read_ch = read_ch;
    if (last_read_at > 0) {
        e.last_read_at = last_read_at;
    }
    // same composition as the stats path (design D2): read_ch drives bit1,
    // the category + dim bits ride through untouched. is_new is "previously
    // set OR grew in this call" (the rollup never observes the chapter list).
    const int news = e.status_flags & static_cast<int>(std::to_underlying(StatusFlag::kNew));
    e.status_flags = compose_flags(e, "", news != 0);
    // Guarded like the stats path: a rollup that re-learns the same counts must
    // not rewrite the row (every row-served detail open runs this).
    if (e.read_ch != old_read || e.status_flags != old_flags || e.last_read_at != old_last) {
        impl_->storage.replace(e);
    }
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

