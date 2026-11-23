#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <glaze/glaze.hpp>
using namespace sqlite_orm;

static bool title_meta_row_bad(const std::string& json) {
    if (json.empty()) {
        return false;
    }
    glz::generic g;
    return glz::read_json(g, json) != glz::error_code::none;
}

std::optional<Store::TitleMetaRow> Store::title_meta_get(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"title_meta_get"};
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr && e->meta.has_value()) {
            const auto& r = *e->meta;
            if (title_meta_row_bad(r.fetched) || title_meta_row_bad(r.chosen)) {
                return std::nullopt;
            }
            return TitleMetaRow{
                .plugin = r.plugin, .slug = r.slug, .fetched = r.fetched, .chosen = r.chosen, .fetched_at = r.fetched_at};
        }
        return std::nullopt;
    }

    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    if (title_meta_row_bad(r.fetched) || title_meta_row_bad(r.chosen)) {
        spdlog::warn("[store] title_meta row {}/{}: corrupt JSON, treating as absent", plugin, slug);
        return std::nullopt;
    }
    return TitleMetaRow{
        .plugin = r.plugin, .slug = r.slug, .fetched = r.fetched, .chosen = r.chosen, .fetched_at = r.fetched_at};
}

std::map<std::pair<std::string, std::string>, Store::TitleMetaRow> Store::title_meta_get_many(
    const std::vector<std::pair<std::string, std::string>>& keys) {
    const store_detail::OpTrace _op{"title_meta_get_many"};
    std::map<std::pair<std::string, std::string>, Store::TitleMetaRow> out;
    // group by plugin: one indexed query per plugin with the slug set as IN --
    // a raw pair-IN statement is not worth it at library scale.
    std::map<std::string, std::vector<std::string>> by_plugin;
    for (const auto& key : keys) {
        by_plugin[key.first].push_back(key.second);
    }
    for (const auto& [plugin, slugs] : by_plugin) {
        auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
            where(c(&store_detail::TitleMetaEntity::plugin) == plugin &&
                  in(&store_detail::TitleMetaEntity::slug, slugs)));
        for (const auto& r : rows) {
            if (title_meta_row_bad(r.fetched) || title_meta_row_bad(r.chosen)) {
                continue; // corrupt reads as absent (parity with title_meta_get)
            }
            out.try_emplace({r.plugin, r.slug}, TitleMetaRow{.plugin = r.plugin,
                                                             .slug = r.slug,
                                                             .fetched = r.fetched,
                                                             .chosen = r.chosen,
                                                             .fetched_at = r.fetched_at});
        }
    }
    return out;
}

void Store::title_meta_put_fetched(const std::string& plugin, const std::string& slug, const std::string& fetched_json,
                                   std::int64_t fetched_at) {
    const store_detail::OpTrace _op{"title_meta_put_fetched"};
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));    if (!saved_exists(plugin, slug)) {
        auto& e = ephem_create(plugin, slug);
        if (!e.meta.has_value()) {
            e.meta = store_detail::TitleMetaEntity{.plugin = plugin,
                                                   .slug = slug,
                                                   .fetched = fetched_json,
                                                   .chosen = "{}",
                                                   .fetched_at = fetched_at};
        } else {
            e.meta->fetched = fetched_json; // chosen survives a re-fetch
            e.meta->fetched_at = fetched_at;
        }
        return;
    }

    store_detail::TitleMetaEntity row{
        .plugin = plugin, .slug = slug, .fetched = fetched_json, .chosen = "{}", .fetched_at = fetched_at};
    if (!rows.empty()) {
        row.chosen = rows[0].chosen; // chosen survives a re-fetch
    }
    impl_->storage.replace(
        row); // vendored sqlite_orm: insert() is broken here; replace is the write path (saved_manga precedent)
}

void Store::title_meta_put_chosen(const std::string& plugin, const std::string& slug, const std::string& chosen_json) {
    const store_detail::OpTrace _op{"title_meta_put_chosen"};
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));    if (!saved_exists(plugin, slug)) {
        auto& e = ephem_create(plugin, slug);
        if (!e.meta.has_value()) {
            e.meta = store_detail::TitleMetaEntity{.plugin = plugin,
                                                   .slug = slug,
                                                   .fetched = "[]",
                                                   .chosen = chosen_json,
                                                   .fetched_at = 0};
        } else {
            e.meta->chosen = chosen_json; // fetched survives a chosen save
        }
        return;
    }

    store_detail::TitleMetaEntity row{
        .plugin = plugin, .slug = slug, .fetched = "[]", .chosen = chosen_json, .fetched_at = 0};
    if (!rows.empty()) {
        row.fetched = rows[0].fetched; // fetched survives a chosen save
        row.fetched_at = rows[0].fetched_at;
    }
    impl_->storage.replace(row);
}

namespace {
// chosen is a small object; parse, drop the named keys, re-dump. Any failure
// to round-trip keeps the existing row untouched (no silent clobber).
bool chosen_clear(glz::generic& chosen, std::initializer_list<const char*> drop) {
    if (!chosen.is_object()) {
        return false;
    }
    for (const char* k : drop) {
        chosen.get_object().erase(k);
    }
    return true;
}
} // namespace

void Store::title_meta_reset_cover(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"title_meta_reset_cover"};
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr && e->meta.has_value()) {
            glz::generic chosen;
            if (!glz::read_json(chosen, e->meta->chosen) && chosen_clear(chosen, {"cover"})) {
                if (auto out = glz::write_json(chosen)) {
                    e->meta->chosen = std::move(*out);
                }
            }
        }
        return;
    }
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    glz::generic chosen;
    if (glz::read_json(chosen, rows[0].chosen) || !chosen_clear(chosen, {"cover"})) {
        return;
    }
    if (auto out = glz::write_json(chosen)) {
        rows[0].chosen = std::move(*out);
        impl_->storage.replace(rows[0]);
    }
}

void Store::title_meta_reset_detail(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"title_meta_reset_detail"};
    if (!saved_exists(plugin, slug)) {
        if (auto* e = ephem_find(plugin, slug); e != nullptr && e->meta.has_value()) {
            glz::generic chosen;
            if (!glz::read_json(chosen, e->meta->chosen) &&
                !chosen_clear(chosen, {"title", "synopsis", "artist", "author", "tags_add", "tags_hidden", "related_add"})) {
                // round-trip failure keeps the row untouched (parity with SQL path)
            } else if (auto out = glz::write_json(chosen)) {
                e->meta->chosen = std::move(*out);
            }
        }
        return;
    }
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) {
        return;
    }
    glz::generic chosen;
    if (glz::read_json(chosen, rows[0].chosen) ||
        !chosen_clear(chosen, {"title", "synopsis", "artist", "author", "tags_add", "tags_hidden", "related_add"})) {
        return;
    }
    if (auto out = glz::write_json(chosen)) {
        rows[0].chosen = std::move(*out);
        impl_->storage.replace(rows[0]);
    }
}
