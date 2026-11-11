#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <glaze/glaze.hpp>
#include <optional>
#include <string>
#include <spdlog/spdlog.h>
using namespace sqlite_orm;

static bool title_meta_row_bad(const std::string& json) {
    if (json.empty()) return false;
    glz::generic g;
    return glz::read_json(g, json) != glz::error_code::none;
}

std::optional<Store::TitleMetaRow> Store::title_meta_get(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) return std::nullopt;
    const auto& r = rows[0];
    if (title_meta_row_bad(r.fetched) || title_meta_row_bad(r.chosen)) {
        spdlog::warn("[store] title_meta row {}/{}: corrupt JSON, treating as absent", plugin, slug);
        return std::nullopt;
    }
    return TitleMetaRow{r.plugin, r.slug, r.fetched, r.chosen, r.fetched_at};
}

void Store::title_meta_put_fetched(const std::string& plugin, const std::string& slug,
                                   const std::string& fetched_json, std::int64_t fetched_at) {
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (!rows.empty()) {
        rows[0].fetched = fetched_json;
        rows[0].fetched_at = fetched_at;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(store_detail::TitleMetaEntity{plugin, slug, fetched_json, "{}", fetched_at});
}

void Store::title_meta_put_chosen(const std::string& plugin, const std::string& slug, const std::string& chosen_json) {
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (!rows.empty()) {
        rows[0].chosen = chosen_json;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(store_detail::TitleMetaEntity{plugin, slug, "[]", chosen_json, 0});
}

namespace {
// chosen is a small object; parse, drop the named keys, re-dump. Any failure
// to round-trip keeps the existing row untouched (no silent clobber).
bool chosen_clear(glz::generic& chosen, std::initializer_list<const char*> drop) {
    if (!chosen.is_object()) return false;
    for (const char* k : drop) chosen.get_object().erase(k);
    return true;
}
} // namespace

void Store::title_meta_reset_cover(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) return;
    glz::generic chosen;
    if (glz::read_json(chosen, rows[0].chosen) || !chosen_clear(chosen, {"cover"})) return;
    if (auto out = glz::write_json(chosen)) {
        rows[0].chosen = std::move(*out);
        impl_->storage.update(rows[0]);
    }
}

void Store::title_meta_reset_detail(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::TitleMetaEntity>(
        where(c(&store_detail::TitleMetaEntity::plugin) == plugin && c(&store_detail::TitleMetaEntity::slug) == slug));
    if (rows.empty()) return;
    glz::generic chosen;
    if (glz::read_json(chosen, rows[0].chosen) || !chosen_clear(chosen, {"title", "synopsis", "artist", "author", "tags_add", "tags_hidden"})) return;
    if (auto out = glz::write_json(chosen)) {
        rows[0].chosen = std::move(*out);
        impl_->storage.update(rows[0]);
    }
}
