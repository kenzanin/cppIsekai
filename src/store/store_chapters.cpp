#include "parse_num.hpp"
#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <vector>

using namespace sqlite_orm;

#include <spdlog/spdlog.h>

// --- chapter-store (chapter-state): identity rows + per-chapter flag bitfield ---

namespace {

// store-contention C3: stable-across-rebuilds seed fingerprint (FNV-1a, NOT
// std::hash) so the plugin_state value compares after restarts. Covers id,
// number, title, url -- the fields the upsert would rewrite.
std::string chapters_fingerprint(const std::vector<ChapterSeed>& rows) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h](unsigned char c) { h ^= c; h *= 1099511628211ull; };
    const auto mix_str = [&mix](const std::string& s, unsigned char sep) {
        for (char c : s) {
            mix(static_cast<unsigned char>(c));
        }
        mix(sep);
    };
    for (const auto& r : rows) {
        mix_str(r.chapter_id, 0x1f);
        mix_str(std::to_string(r.number), 0x1f);
        mix_str(r.title, 0x1e);
        mix_str(r.url, 0x1d);
        mix_str(r.release_at, 0x1c);
    }
    return std::to_string(rows.size()) + ":" + std::to_string(h);
}

} // namespace

int Store::chapters_upsert(const std::string& plugin, const std::string& slug,
                            const std::vector<ChapterSeed>& rows) {
    const store_detail::OpTrace _op{"chapters_upsert"};
    if (plugin.empty() || slug.empty()) {
        return 0;
    }
    auto& st = impl_->storage;
    const std::int64_t now = std::time(nullptr);
    // store-contention C3: materialize feeds every seen chapters envelope here;
    // an unchanged seed set would rewrite identical state (~100 ms mutexed).
    // Fingerprint lives in plugin_state, written inside the txn below; a lost
    // one only costs one redundant pass. Flags never appear here, so skipping
    // preserves them by construction.
    const std::string fp = chapters_fingerprint(rows);
    const std::string fp_key = std::string(kChaptersFpPrefix) + plugin + "/" + slug;
    if (const auto stored = state_get(kStateHost, fp_key); stored && *stored == fp) {
        return 0;
    }
    // Reads happen BEFORE the write transaction: a deferred transaction that
    // reads and then writes upgrades its snapshot, and the upgrade surfaces as
    // an instant "database is locked" (SQLITE_BUSY_SNAPSHOT) whenever another
    // connection committed in between -- which is what wedged this method.
    auto existing = st.get_all<store_detail::ChapterEntity>(
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug));
    std::map<std::string, store_detail::ChapterEntity> known;
    for (auto& e : existing) {
        known.emplace(e.chapter_id, std::move(e));
    }
    std::vector<store_detail::ChapterEntity> writes;
    writes.reserve(rows.size());
    int inherited = 0;
    for (const auto& r : rows) {
        if (r.chapter_id.empty()) {
            continue;
        }
        if (auto it = known.find(r.chapter_id); it != known.end()) {
            auto e = it->second; // flags ride through untouched
            e.number = r.number;
            e.title = r.title;
            e.url = r.url;
            e.release_at = r.release_at;
            e.updated_at = now;
            writes.push_back(std::move(e));
            continue;
        }
        store_detail::ChapterEntity e{.plugin = plugin,
                                      .slug = slug,
                                      .chapter_id = r.chapter_id,
                                      .number = r.number,
                                      .title = r.title,
                                      .url = r.url,
                                      .release_at = r.release_at,
                                      .flags = 0,
                                      .updated_at = now};
        // chapter-state D2: one-shot legacy inheritance on first sight -- the
        // host/* keys carry no manga identity, so the parse claiming the id is
        // the attribution. skip=1 -> bit1; read>=total(>0) -> bit0.
        std::int64_t legacy = 0;        if (const auto s = state_get(kStateHost, std::string(kSkipPrefix) + r.chapter_id); s && *s == "1") {
            legacy |= std::to_underlying(ChapterFlag::kSkip);
        }
        if (const auto total = state_get(kStateHost, std::string(kTotalPrefix) + r.chapter_id);
            total && !total->empty() && *total != "0") {
            if (const auto rd = state_get(kStateHost, std::string(kReadPrefix) + r.chapter_id);
                rd && !rd->empty()) {
                if (const int p = parse::to_int(*rd), t = parse::to_int(*total); p >= t && p > 0) {
                    legacy |= std::to_underlying(ChapterFlag::kRead);
                }
            }
        }
        e.flags = static_cast<int>(legacy);
        inherited += legacy != 0 ? 1 : 0;
        writes.push_back(std::move(e));
    }
    if (writes.empty()) {
        return 0;
    }
    store_detail::TxGuard tx{st};
    st.begin_transaction();
    for (auto& e : writes) {
        // composite-PK upsert for new rows; insert() misbinds NOT NULL cols here
        known.contains(e.chapter_id) ? st.update(e) : st.replace(e);
    }
    st.replace(store_detail::StateEntity{.plugin_id = kStateHost, .key = fp_key, .value = fp});
    tx.commit();
    if (inherited > 0) {
        spdlog::debug("chapter-store: {} legacy flag(s) inherited for {}/{}", inherited, plugin, slug);
    }
    return static_cast<int>(writes.size());
}

std::vector<ChapterSeed> Store::chapters_list(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"chapters_list"};
    if (plugin.empty() || slug.empty()) {
        return {};
    }
    auto rows = impl_->storage.get_all<store_detail::ChapterEntity>(
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug),
        order_by(&store_detail::ChapterEntity::number));
    std::vector<ChapterSeed> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(ChapterSeed{.chapter_id = r.chapter_id,
                                  .number = r.number,
                                  .title = r.title,
                                  .url = r.url,
                                  .release_at = r.release_at});
    }
    return out;
}

void Store::chapter_pages_replace(const std::string& plugin, const std::string& slug, const std::string& chapter_id,
                                  const std::vector<std::string>& urls) {
    const store_detail::OpTrace _op{"chapter_pages_replace"};
    if (plugin.empty() || slug.empty() || chapter_id.empty()) {
        return;
    }
    auto& st = impl_->storage;
    store_detail::TxGuard tx{st};
    st.begin_transaction();
    st.remove_all<store_detail::ChapterPageEntity>(
        where(c(&store_detail::ChapterPageEntity::plugin) == plugin &&
              c(&store_detail::ChapterPageEntity::slug) == slug &&
              c(&store_detail::ChapterPageEntity::chapter_id) == chapter_id));
    for (std::size_t i = 0; i < urls.size(); ++i) {
        st.replace(store_detail::ChapterPageEntity{.plugin = plugin,
                                                  .slug = slug,
                                                  .chapter_id = chapter_id,
                                                  .idx = static_cast<int>(i),
                                                  .url = urls[i]});
    }
    tx.commit();
}

std::map<std::string, int> Store::chapter_flags_apply(const std::string& plugin, const std::string& slug,
                                                      const std::vector<std::string>& ids, bool all, int set,
                                                      int clear) {
    const store_detail::OpTrace _op{"chapter_flags_apply"};
    std::map<std::string, int> out;
    if (plugin.empty() || slug.empty() || (!all && ids.empty())) {
        return out;
    }
    auto& st = impl_->storage;
    std::vector<store_detail::ChapterEntity> rows;
    if (all) {
        rows = st.get_all<store_detail::ChapterEntity>(
            where(c(&store_detail::ChapterEntity::plugin) == plugin &&
                  c(&store_detail::ChapterEntity::slug) == slug));
    } else {
        std::vector<std::string> want(ids);
        rows = st.get_all<store_detail::ChapterEntity>(
            where(c(&store_detail::ChapterEntity::plugin) == plugin &&
                  c(&store_detail::ChapterEntity::slug) == slug &&
                  in(&store_detail::ChapterEntity::chapter_id, std::move(want))));
    }
    const std::int64_t now = std::time(nullptr);
    store_detail::TxGuard tx{st};
    st.begin_transaction();
    for (auto& e : rows) {
        e.flags = (e.flags | set) & ~clear;
        e.updated_at = now;
        st.update(e);
        out.emplace(e.chapter_id, e.flags);
    }
    tx.commit();
    return out;
}

std::optional<int> Store::chapter_flags_get(const std::string& plugin, const std::string& slug,
                                            const std::string& chapter_id) {
    const store_detail::OpTrace _op{"chapter_flags_get"};
    if (plugin.empty() || slug.empty() || chapter_id.empty()) {
        return std::nullopt;
    }
    auto rows = impl_->storage.select(&store_detail::ChapterEntity::flags,
                                      where(c(&store_detail::ChapterEntity::plugin) == plugin &&
                                            c(&store_detail::ChapterEntity::slug) == slug &&
                                            c(&store_detail::ChapterEntity::chapter_id) == chapter_id));
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows.front();
}

std::map<std::string, int> Store::chapter_flags_list(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"chapter_flags_list"};
    std::map<std::string, int> out;
    if (plugin.empty() || slug.empty()) {
        return out;
    }
    auto rows = impl_->storage.select(
        columns(&store_detail::ChapterEntity::chapter_id, &store_detail::ChapterEntity::flags),
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug));
    for (auto& [id, flags] : rows) {
        out.emplace(std::move(id), flags);
    }
    return out;
}

int Store::chapter_pages_count(const std::string& plugin, const std::string& slug, const std::string& chapter_id) {
    const store_detail::OpTrace _op{"chapter_pages_count"};
    if (plugin.empty() || slug.empty() || chapter_id.empty()) {
        return 0;
    }
    return static_cast<int>(impl_->storage.count<store_detail::ChapterPageEntity>(
        where(c(&store_detail::ChapterPageEntity::plugin) == plugin &&
              c(&store_detail::ChapterPageEntity::slug) == slug &&
              c(&store_detail::ChapterPageEntity::chapter_id) == chapter_id)));
}
