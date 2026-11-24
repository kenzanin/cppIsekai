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


std::map<std::string, int> Store::chapter_flags_apply(const std::string& plugin, const std::string& slug,
                                                      const std::vector<std::string>& ids, bool all, int set,
                                                      int clear) {
    const store_detail::OpTrace _op{"chapter_flags_apply"};
    std::map<std::string, int> out;
    if (plugin.empty() || slug.empty() || (!all && ids.empty())) {
        return out;
    }
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr) {
            const std::int64_t now = std::time(nullptr);
            for (auto& [id, c] : e->chapters) {
                if (!all && std::ranges::find(ids, id) == ids.end()) {
                    continue;
                }
                c.flags = (c.flags | set) & ~clear;
                c.updated_at = now;
                out.emplace(id, c.flags);
            }
        }
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
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr) {
            const auto c = e->chapters.find(chapter_id);
            if (c == e->chapters.end()) {
                return std::nullopt;
            }
            return c->second.flags;
        }
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
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr) {
            for (const auto& [id, c] : e->chapters) {
                out.emplace(id, c.flags);
            }
        }
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

std::map<std::string, int> Store::chapter_read_max_list(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"chapter_read_max_list"};
    std::map<std::string, int> out;
    if (plugin.empty() || slug.empty()) {
        return out;
    }
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr) {
            for (const auto& [id, c] : e->chapters) {
                out.emplace(id, c.read_max);
            }
        }
        return out;
    }
    auto rows = impl_->storage.select(
        columns(&store_detail::ChapterEntity::chapter_id, &store_detail::ChapterEntity::read_max),
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug));
    for (auto& [id, read_max] : rows) {
        out.emplace(std::move(id), read_max);
    }
    return out;
}

int Store::chapter_read_max_bump(const std::string& plugin, const std::string& slug,
                                 const std::string& chapter_id, int page) {
    const store_detail::OpTrace _op{"chapter_read_max_bump"};
    if (plugin.empty() || slug.empty() || chapter_id.empty() || page < 0) {
        return 0;
    }
    if (!saved_exists(plugin, slug)) {
        auto* e = ephem_find(plugin, slug);
        if (e == nullptr) {
            return 0; // no rows to carry progress on; nothing to bump
        }
        auto c = e->chapters.find(chapter_id);
        if (c == e->chapters.end()) {
            return 0;
        }
        const int now = std::max(c->second.read_max, page);
        if (now == c->second.read_max && (c->second.flags & std::to_underlying(ChapterFlag::kRead)) != 0) {
            return now;
        }
        c->second.read_max = now;
        if ((c->second.flags & std::to_underlying(ChapterFlag::kRead)) == 0) {
            if (const int total = static_cast<int>(c->second.pages.size()); total > 0 && now >= total) {
                c->second.flags |= std::to_underlying(ChapterFlag::kRead);
            }
        }
        c->second.updated_at = std::time(nullptr);
        return now;
    }
    auto rows = impl_->storage.get_all<store_detail::ChapterEntity>(
        where(c(&store_detail::ChapterEntity::plugin) == plugin && c(&store_detail::ChapterEntity::slug) == slug &&
              c(&store_detail::ChapterEntity::chapter_id) == chapter_id));
    if (rows.empty()) {
        return 0;
    }
    auto& e = rows.front();
    const int now = std::max(e.read_max, page);
    if (now == e.read_max && (e.flags & std::to_underlying(ChapterFlag::kRead)) != 0) {
        return now; // nothing to merge, already read
    }
    e.read_max = now;
    // chapter-store 3.3/D5: last page turned == read, no client flagging.
    // count==0 = pages never parsed; no threshold to reach yet.
    if ((e.flags & std::to_underlying(ChapterFlag::kRead)) == 0) {
        if (const int total = chapter_pages_count(plugin, slug, chapter_id); total > 0 && now >= total) {
            e.flags |= std::to_underlying(ChapterFlag::kRead);
        }
    }
    e.updated_at = std::time(nullptr);
    impl_->storage.update(e);
    return now;
}

int Store::chapter_pages_count(const std::string& plugin, const std::string& slug, const std::string& chapter_id) {
    const store_detail::OpTrace _op{"chapter_pages_count"};
    if (plugin.empty() || slug.empty() || chapter_id.empty()) {
        return 0;
    }

    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr) {
            const auto c = e->chapters.find(chapter_id);
            return c != e->chapters.end() ? static_cast<int>(c->second.pages.size()) : 0;
        }
        return 0;
    }
    return static_cast<int>(impl_->storage.count<store_detail::ChapterPageEntity>(
        where(c(&store_detail::ChapterPageEntity::plugin) == plugin &&
              c(&store_detail::ChapterPageEntity::slug) == slug &&
              c(&store_detail::ChapterPageEntity::chapter_id) == chapter_id)));
}
