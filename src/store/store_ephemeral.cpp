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

namespace {

std::pair<std::string, std::string> key_of(const std::string& plugin, const std::string& slug) {
    return {plugin, slug};
}

} // namespace

namespace store_detail {

// The ephemeral pair copy is a straight struct->row mapping; kept next to the
// entity shapes so a field addition fails to compile here instead of silently
// dropping state on promote.
void ephem_copy_chapters(StoreImpl& impl, const std::string& plugin, const std::string& slug,
                         const std::map<std::string, EphemChapter>& chapters) {
    auto& st = impl.storage;
    for (const auto& [id, c] : chapters) {
        const store_detail::ChapterEntity row{.plugin = plugin,
                                              .slug = slug,
                                              .chapter_id = id,
                                              .number = c.number,
                                              .title = c.title,
                                              .url = c.url,
                                              .release_at = c.release_at,
                                              .flags = c.flags,
                                              .read_max = c.read_max,
                                              .updated_at = c.updated_at};
        st.replace(row);
        for (const auto& [idx, url] : c.pages) {
            st.replace(store_detail::ChapterPageEntity{.plugin = plugin,
                                                       .slug = slug,
                                                       .chapter_id = id,
                                                       .idx = idx,
                                                       .url = url});
        }
    }
}

} // namespace store_detail

bool Store::saved_exists(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"saved_exists"};
    // sqlite_orm EXISTS short-circuits on the composite PK instead of counting
    // matches; still one indexed statement, but the SQL states intent clearly.
    // sqlite_orm 1.9 only accepts EXISTS inside WHERE (not as a top-level
    // select result), so wrap it: one indexed statement, short-circuiting.
    const auto rows = impl_->storage.select(count<store_detail::SavedMangaEntity>(),
                                            where(exists(select(&store_detail::SavedMangaEntity::plugin,
                                                                where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
                                                                      c(&store_detail::SavedMangaEntity::slug) == slug)))));
    return !rows.empty() && rows.front() != 0;
}

store_detail::EphemPair* Store::ephem_find(const std::string& plugin, const std::string& slug) {
    const auto key = key_of(plugin, slug);
    const auto it = impl_->ephem.find(key);
    if (it == impl_->ephem.end()) {
        return nullptr;
    }
    impl_->ephem_touch(key);
    return &it->second;
}

store_detail::EphemPair& Store::ephem_create(const std::string& plugin, const std::string& slug) {
    const auto key = key_of(plugin, slug);
    impl_->ephem_touch(key);
    return impl_->ephem[key];
}

void Store::ephem_erase(const std::string& plugin, const std::string& slug) {
    const auto key = key_of(plugin, slug);
    impl_->ephem.erase(key);
    impl_->ephem_lru.remove(key);
}

void Store::ephem_copy_into(const std::string& plugin, const std::string& slug) {
    const auto it = impl_->ephem.find(key_of(plugin, slug));
    if (it == impl_->ephem.end()) {
        return;
    }
    auto& st = impl_->storage;
    const auto& e = it->second;
    store_detail::ephem_copy_chapters(*impl_, plugin, slug, e.chapters);
    if (e.position) {
        st.replace(*e.position);
    }
    if (e.meta) {
        st.replace(*e.meta);
    }
    // host columns: merge, never stomp a non-empty stored value with empty
    auto rows = st.get_all<store_detail::SavedMangaEntity>(
        where(c(&store_detail::SavedMangaEntity::plugin) == plugin &&
              c(&store_detail::SavedMangaEntity::slug) == slug));
    if (!rows.empty()) {
        auto& s = rows.front();
        if (e.novel) {
            s.novel = 1;
        }
        if (!e.detail_url.empty()) {
            s.detail_url = e.detail_url;
        }
        if (!e.chapters_fp.empty()) {
            s.chapters_fp = e.chapters_fp;
        }
        st.update(s);
    }
}

void Store::ephem_promote(const std::string& plugin, const std::string& slug) {
    if (impl_->ephem.find(key_of(plugin, slug)) == impl_->ephem.end()) {
        return;
    }
    {
        store_detail::TxGuard tx{impl_->storage};
        impl_->storage.begin_transaction();
        ephem_copy_into(plugin, slug);
        tx.commit();
    }
    ephem_erase(plugin, slug);
}

