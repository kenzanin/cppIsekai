#include "store.hpp"
#include "store_busy.hpp"
#include "store_internal.hpp"

#include <ctime>
#include <string>
#include <string_view>
#include <vector>

using namespace sqlite_orm;

std::vector<Plugin> Store::plugins() {
    const store_detail::OpTrace _op{"plugins"};
    auto rows = impl_->storage.select(
        columns(&store_detail::PluginEntity::id, &store_detail::PluginEntity::name,
                sqlite_orm::count(&store_detail::TitleEntity::id), &store_detail::PluginEntity::plugin_id,
                &store_detail::PluginEntity::version, &store_detail::PluginEntity::enabled,
                &store_detail::PluginEntity::runtime, &store_detail::PluginEntity::capabilities),
        left_join<store_detail::TitleEntity>(
            on(c(&store_detail::TitleEntity::plugin_id) == &store_detail::PluginEntity::id)),
        group_by(&store_detail::PluginEntity::id), order_by(&store_detail::PluginEntity::name));
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
    const store_detail::OpTrace _op{"upsert_plugin"};
    auto rows =
        impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::plugin_id) == id));
    if (!rows.empty()) {
        rows[0].name = name;
        rows[0].version = version;
        rows[0].capabilities = capabilities;
        rows[0].runtime = runtime;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(store_detail::PluginEntity{.id = 0,
                                                     .name = name,
                                                     .plugin_id = id,
                                                     .version = version,
                                                     .capabilities = capabilities,
                                                     .enabled = 1,
                                                     .runtime = runtime});
}

void Store::set_plugin_enabled(std::int64_t id, bool on) {
    const store_detail::OpTrace _op{"set_plugin_enabled"};
    impl_->storage.update_all(set(c(&store_detail::PluginEntity::enabled) = on ? 1 : 0),
                              where(c(&store_detail::PluginEntity::id) == id));
}

bool Store::plugin_enabled(const std::string& plugin_id) {
    const store_detail::OpTrace _op{"plugin_enabled"};
    auto rows = impl_->storage.get_all<store_detail::PluginEntity>(
        where(c(&store_detail::PluginEntity::plugin_id) == plugin_id));
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
        if (ch == '%' || ch == '_' || ch == '\\') {
            out += '\\';
        }
        out += ch;
    }
    return out + "%";
}
} // namespace

int Store::count(const std::string& plugin, const std::string& query) {
    const store_detail::OpTrace _op{"count"};
    const auto pattern = like_pattern(query);
    if (plugin.empty()) {
        return impl_->storage.count<store_detail::TitleEntity>(
            where(like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\"))));
    }

    auto rows =
        impl_->storage.get_all<store_detail::PluginEntity>(where(c(&store_detail::PluginEntity::name) == plugin));
    if (rows.empty()) {
        return 0;
    }
    return impl_->storage.count<store_detail::TitleEntity>(
        where(c(&store_detail::TitleEntity::plugin_id) == rows[0].id &&
              like(&store_detail::TitleEntity::name, pattern).escape(std::string("\\"))));
}

std::vector<Title> Store::search(const std::string& plugin, const std::string& query, int limit, int offset) {
    const store_detail::OpTrace _op{"search"};
    const auto pattern = like_pattern(query);
    // LIKE '%' on plugin matches every row: one condition shape, no empty-branch special case.
    auto rows = impl_->storage.select(
        columns(&store_detail::TitleEntity::id, &store_detail::TitleEntity::name, &store_detail::PluginEntity::name),
        inner_join<store_detail::PluginEntity>(
            on(c(&store_detail::PluginEntity::id) == &store_detail::TitleEntity::plugin_id)),
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
    const store_detail::OpTrace _op{"state_get"};
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id && c(&store_detail::StateEntity::key) == key));
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows[0].value;
}

void Store::state_set(const std::string& plugin_id, const std::string& key, const std::string& value) {
    const store_detail::OpTrace _op{"state_set"};
    impl_->storage.replace(store_detail::StateEntity{
        .plugin_id = plugin_id, .key = key, .value = value}); // INSERT OR REPLACE on the composite pk: upsert
}

std::vector<std::string> Store::state_keys_value(const std::string& plugin_id, const std::string& key_prefix,
                                                 const std::string& value) {
    const store_detail::OpTrace _op{"state_keys_value"};
    std::string pat;
    pat.reserve(key_prefix.size() + 1);
    for (char ch : key_prefix) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            pat += '\\';
        }
        pat += ch;
    }
    pat += '%';
    auto rows = impl_->storage.get_all<store_detail::StateEntity>(
        where(c(&store_detail::StateEntity::plugin_id) == plugin_id &&
              like(&store_detail::StateEntity::key, pat).escape(std::string("\\")) &&
              c(&store_detail::StateEntity::value) == value));
    std::vector<std::string> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        out.push_back(r.key);
    }
    return out;
}

void Store::position_set(const ReaderPosition& pos) {
    const store_detail::OpTrace _op{"position_set"};
    // REPLACE on the (plugin, slug) PK: the route layers optional fields over
    // the stored row, this just persists the result. updated_at stamped here.
    impl_->storage.replace(store_detail::ReaderPositionEntity{
        .plugin = pos.plugin,
        .slug = pos.slug,
        .chapter_id = pos.chapter_id,
        .chapter_number = pos.chapter_number,
        .page = pos.page,
        .mode = pos.mode,
        .direction = pos.direction,
        .zoom = pos.zoom,
        .updated_at = static_cast<std::int64_t>(std::time(nullptr)),
    });
}

std::optional<ReaderPosition> Store::position_get(const std::string& plugin, const std::string& slug) {
    const store_detail::OpTrace _op{"position_get"};
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        where(c(&store_detail::ReaderPositionEntity::plugin) == plugin &&
              c(&store_detail::ReaderPositionEntity::slug) == slug));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return ReaderPosition{
        .plugin = r.plugin,
        .slug = r.slug,
        .chapter_id = r.chapter_id,
        .chapter_number = r.chapter_number,
        .page = r.page,
        .mode = r.mode,
        .direction = r.direction,
        .zoom = r.zoom,
        .updated_at = r.updated_at,
    };
}

std::optional<ReaderPosition> Store::position_last() {
    const store_detail::OpTrace _op{"position_last"};
    auto rows = impl_->storage.get_all<store_detail::ReaderPositionEntity>(
        order_by(&store_detail::ReaderPositionEntity::updated_at).desc(), limit(1));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& r = rows[0];
    return ReaderPosition{
        .plugin = r.plugin,
        .slug = r.slug,
        .chapter_id = r.chapter_id,
        .chapter_number = r.chapter_number,
        .page = r.page,
        .mode = r.mode,
        .direction = r.direction,
        .zoom = r.zoom,
        .updated_at = r.updated_at,
    };
}
