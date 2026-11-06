#pragma once
// The only SQL in the program lives in this class. Callers get methods, never
// statement text. sqlite_orm (vendored, Pimpl-hidden) under the hood.
// ponytail: sqlite_orm is not thread-safe; all calls must come from the db worker.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Plugin {
    std::int64_t id{};
    std::string name;
    int titles{};
    std::string plugin_id; // stable plugin key (folder id); empty for seed rows
};

struct Title {
    std::int64_t id{};
    std::string name;
    std::string plugin;
};

struct StoreImpl;

class Store {
public:
    explicit Store(const std::string& path);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    std::vector<Plugin> plugins();
    // Register/upsert a discovered plugin keyed by its stable id.
    void upsert_plugin(const std::string& id, const std::string& name, const std::string& version,
                       const std::string& capabilities);
    int count(const std::string& plugin, const std::string& query);
    std::vector<Title> search(const std::string& plugin, const std::string& query, int limit, int offset);

private:
    std::unique_ptr<StoreImpl> impl_;
};
