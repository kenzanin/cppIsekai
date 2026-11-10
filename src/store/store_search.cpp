#include "store.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <stdexcept>
#include <utility>
using namespace sqlite_orm;

std::vector<Plugin> Store::plugins() {
    auto rows = impl_->storage.select(
        columns(&store_detail::PluginEntity::id, &store_detail::PluginEntity::name, sqlite_orm::count(&store_detail::TitleEntity::id), &store_detail::PluginEntity::plugin_id,
                &store_detail::PluginEntity::version, &store_detail::PluginEntity::enabled, &store_detail::PluginEntity::runtime,
                &store_detail::PluginEntity::capabilities),
        left_join<store_detail::TitleEntity>(on(c(&store_detail::TitleEntity::plugin_id) == &store_detail::PluginEntity::id)), group_by(&store_detail::PluginEntity::id),
        order_by(&store_detail::PluginEntity::name));
    std::vector<Plugin> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        Plugin p;
        p.id = std::get<0>(row);
        p.name = std::get<1>(row);
        p.titles = std::get<2>(row);
        p.plugin_id = std::get<3>(row);
        p.version = std::get<4>(row);
        p.enabled = std::get<5>(row) != 0;
        p.runtime = std::get<6>(row);
        p.caps = std::get<7>(row);
        out.push_back(std::move(p));
    }
    return out;
}

void Store::upsert_plugin(const std::string& id, const std::string& name, const std::string& version,
                          const std::string& capabilities, const std::string& runtime) {
    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::plugin_id) == id));
    if (!rows.empty()) {
        rows[0].name = name;
        rows[0].version = version;
        rows[0].capabilities = capabilities;
        rows[0].runtime = runtime;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(store_detail::PluginEntity{0, name, id, version, capabilities, 1, runtime});
}

void Store::set_plugin_enabled(std::int64_t id, bool on) {
    impl_->storage.update_all(set(c(&store_detail::PluginEntity::enabled) = on ? 1 : 0), where(c(&store_detail::PluginEntity::id) == id));
}

bool Store::plugin_enabled(const std::string& plugin_id) {
    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::plugin_id) == plugin_id));
    if (rows.empty()) {
        return true; // unregistered id: nothing to gate
    }
    return rows[0].enabled != 0;
}

namespace {
// LIKE pattern with %, _ and \\ escaped so user query stays literal; pair with .escape("\\").
std::string like_pattern(const std::string& query) {
    std::string out = "%";
    for (char ch : query) {
        if (ch == '%' || ch == '_' || ch == '\\')
            out += '\\';
        out += ch;
    }
    return out + "%";
}
} // namespace

int Store::count(const std::string& plugin, const std::string& query) {
    const auto pattern = like_pattern(query);
    if (plugin.empty())
        return static_cast<int>(
            impl_->storage.count<store_detail::TitleEntity>(where(like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\")))));

    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::name) == plugin));
    if (rows.empty())
        return 0;
    return static_cast<int>(impl_->storage.count<store_detail::TitleEntity>(where(
        c(&store_detail::TitleEntity::plugin_id) == rows[0].id && like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\")))));
}

std::vector<Title> Store::search(const std::string& plugin, const std::string& query, int limit, int offset) {
    const auto pattern = like_pattern(query);
    // LIKE '%' on plugin matches every row: one condition shape, no empty-branch special case.
    auto rows = impl_->storage.select(
        columns(&store_detail::TitleEntity::id, &store_detail::TitleEntity::name, &store_detail::PluginEntity::name),
        inner_join<store_detail::PluginEntity>(on(c(&store_detail::PluginEntity::id) == &store_detail::TitleEntity::plugin_id)),
        where(like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\")) &&
              like(&store_detail::PluginEntity::name, plugin.empty() ? "%" : plugin)),
        order_by(&store_detail::TitleEntity::name),
        // v1.9.1: standalone offset() serializer is incomplete; the limit(lim, offset) overload is the supported form.
        sqlite_orm::limit(limit, sqlite_orm::offset(offset)));
    std::vector<Title> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        Title t;
        t.id = std::get<0>(row);
        t.name = std::get<1>(row);
        t.plugin = std::get<2>(row);
        out.push_back(std::move(t));
    }
    return out;
}

std::optional<std::string> Store::state_get(const std::string& plugin_id, const std::string& key) {
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id && c(&store_detail::StateEntity::key) == key));
    if (rows.empty())
        return std::nullopt;
    return rows[0].value;
}

void Store::state_set(const std::string& plugin_id, const std::string& key, const std::string& value) {
    impl_->storage.replace(store_detail::StateEntity{plugin_id, key, value}); // INSERT OR REPLACE on the composite pk: upsert
}

void Store::position_set(const ReaderPosition& pos) {
    // REPLACE on the (plugin, slug) PK: the route layers optional fields over
    // the stored row, this just persists the result. updated_at stamped here.
    impl_->storage.replace(store_detail::ReaderPositionEntity{pos.plugin, pos.slug, pos.chapter_id, pos.chapter_number,
                                                pos.page, pos.mode, pos.direction, pos.zoom,
                                                static_cast<std::int64_t>(std::time(nullptr))});
}

std::optional<ReaderPosition> Store::position_get(const std::string& plugin, const std::string& slug) {
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == plugin && c(&store_detail::ReaderPositionEntity::slug) == slug));
    if (rows.empty())
        return std::nullopt;
    const auto& r = rows[0];
    return ReaderPosition{r.plugin, r.slug, r.chapter_id, r.chapter_number, r.page, r.mode, r.direction, r.zoom, r.updated_at};
}


std::optional<ReaderPosition> Store::position_last() {
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(order_by(&store_detail::ReaderPositionEntity::updated_at).desc(), limit(1));
    if (rows.empty())
        return std::nullopt;
    const auto& r = rows[0];
    return ReaderPosition{r.plugin, r.slug, r.chapter_id, r.chapter_number, r.page, r.mode, r.direction, r.zoom, r.updated_at};
}

void Store::save_manga(const SavedManga& m) {
    // REPLACE on the (plugin, slug) PK: refreshes name/cover/added_at on re-save.
    impl_->storage.replace(store_detail::SavedMangaEntity{m.plugin, m.slug, m.name, m.cover,
                                            static_cast<std::int64_t>(std::time(nullptr))});
}

void Store::unsave_manga(const std::string& plugin, const std::string& slug) {
    impl_->storage.remove_all<store_detail::SavedMangaEntity>(
        where(c(&store_detail::SavedMangaEntity::plugin) == plugin && c(&store_detail::SavedMangaEntity::slug) == slug));
}

std::vector<SavedManga> Store::saved_list() {
    // added_at has second resolution, and REPLACE re-stamps it on re-save, so
    // newest-first by added_at alone is stable enough for the single-user grid.
    auto rows = impl_->storage.get_all<store_detail::SavedMangaEntity>(
        order_by(&store_detail::SavedMangaEntity::added_at).desc());
    std::vector<SavedManga> out;
    out.reserve(rows.size());
    for (const auto& r : rows)
        out.push_back(SavedManga{r.plugin, r.slug, r.name, r.cover, r.added_at});
    return out;
}

