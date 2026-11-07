#include "store.hpp"

#include <sqlite_orm/sqlite_orm.h>

#include <stdexcept>
#include <utility>

using namespace sqlite_orm;

namespace {

struct PluginEntity {
    std::int64_t id = 0;
    std::string name;
    std::string plugin_id; // stable plugin key; '' for the seed rows
    std::string version;
    std::string capabilities;
    int enabled = 1;
    std::string runtime;
};

struct TitleEntity {
    std::int64_t id = 0;
    std::int64_t plugin_id = 0;
    std::string name;
};

struct StateEntity {
    std::string plugin_id; // plugin folder id, not plugin.id FK
    std::string key;
    std::string value;
};

inline auto create_storage(const std::string& path) {
    return make_storage(
        path,
        make_table("plugin", make_column("id", &PluginEntity::id, primary_key().autoincrement()),
                   make_column("name", &PluginEntity::name, unique()),
                   make_column("plugin_id", &PluginEntity::plugin_id), make_column("version", &PluginEntity::version),
                   make_column("capabilities", &PluginEntity::capabilities),
                   make_column("enabled", &PluginEntity::enabled, default_value(1)),
                   make_column("runtime", &PluginEntity::runtime)),
        make_table("title", make_column("id", &TitleEntity::id, primary_key().autoincrement()),
                   make_column("plugin_id", &TitleEntity::plugin_id), make_column("name", &TitleEntity::name),
                   foreign_key(&TitleEntity::plugin_id).references(&PluginEntity::id)),
        make_table("plugin_state", make_column("plugin_id", &StateEntity::plugin_id),
                   make_column("key", &StateEntity::key), make_column("value", &StateEntity::value),
                   primary_key(&StateEntity::plugin_id, &StateEntity::key)));
}

using StorageType = decltype(create_storage(""));

} // namespace

struct StoreImpl {
    StorageType storage;
    explicit StoreImpl(const std::string& path) : storage(create_storage(path)) {}
};

Store::Store(const std::string& path) : impl_(std::make_unique<StoreImpl>(path)) {
    impl_->storage.pragma.journal_mode(journal_mode::WAL);
    // FK: pragma_t has no foreign_keys setter in v1.9.1; make_storage enables it
    // automatically because the schema declares a foreign_key.
    impl_->storage.sync_schema();

    if (impl_->storage.count<TitleEntity>() == 0) {
        static constexpr const char* kPlugins[] = {"Asura", "Flame", "Reaper", "Void"};
        auto guard = impl_->storage.transaction_guard();
        for (const char* plugin_name : kPlugins) {
            const auto plugin_id = impl_->storage.insert(PluginEntity{0, plugin_name, "", "", ""});
            for (int n = 1; n <= 12; ++n)
                impl_->storage.insert(
                    TitleEntity{0, plugin_id, std::string(plugin_name) + " Scans #" + std::to_string(n)});
        }
        guard.commit();
    }
}

Store::~Store() = default;

std::vector<Plugin> Store::plugins() {
    auto rows = impl_->storage.select(
        columns(&PluginEntity::id, &PluginEntity::name, sqlite_orm::count(&TitleEntity::id), &PluginEntity::plugin_id,
                &PluginEntity::version, &PluginEntity::enabled, &PluginEntity::runtime,
                &PluginEntity::capabilities),
        left_join<TitleEntity>(on(c(&TitleEntity::plugin_id) == &PluginEntity::id)), group_by(&PluginEntity::id),
        order_by(&PluginEntity::name));
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
    auto rows = impl_->storage.get_all<PluginEntity>(where(c(&PluginEntity::plugin_id) == id));
    if (!rows.empty()) {
        rows[0].name = name;
        rows[0].version = version;
        rows[0].capabilities = capabilities;
        rows[0].runtime = runtime;
        impl_->storage.update(rows[0]);
        return;
    }
    impl_->storage.insert(PluginEntity{0, name, id, version, capabilities, 1, runtime});
}

void Store::set_plugin_enabled(std::int64_t id, bool on) {
    impl_->storage.update_all(set(c(&PluginEntity::enabled) = on ? 1 : 0), where(c(&PluginEntity::id) == id));
}

bool Store::plugin_enabled(const std::string& plugin_id) {
    auto rows = impl_->storage.get_all<PluginEntity>(where(c(&PluginEntity::plugin_id) == plugin_id));
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
            impl_->storage.count<TitleEntity>(where(like(&TitleEntity::name, pattern).escape(std::string("\\")))));

    auto rows = impl_->storage.get_all<PluginEntity>(where(c(&PluginEntity::name) == plugin));
    if (rows.empty())
        return 0;
    return static_cast<int>(impl_->storage.count<TitleEntity>(where(
        c(&TitleEntity::plugin_id) == rows[0].id && like(&TitleEntity::name, pattern).escape(std::string("\\")))));
}

std::vector<Title> Store::search(const std::string& plugin, const std::string& query, int limit, int offset) {
    const auto pattern = like_pattern(query);
    // LIKE '%' on plugin matches every row: one condition shape, no empty-branch special case.
    auto rows = impl_->storage.select(
        columns(&TitleEntity::id, &TitleEntity::name, &PluginEntity::name),
        inner_join<PluginEntity>(on(c(&PluginEntity::id) == &TitleEntity::plugin_id)),
        where(like(&TitleEntity::name, pattern).escape(std::string("\\")) &&
              like(&PluginEntity::name, plugin.empty() ? "%" : plugin)),
        order_by(&TitleEntity::name),
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
    auto rows = impl_->storage.get_all<StateEntity>(
        where(c(&StateEntity::plugin_id) == plugin_id && c(&StateEntity::key) == key));
    if (rows.empty())
        return std::nullopt;
    return rows[0].value;
}

void Store::state_set(const std::string& plugin_id, const std::string& key, const std::string& value) {
    impl_->storage.replace(StateEntity{plugin_id, key, value}); // INSERT OR REPLACE on the composite pk: upsert
}
