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
    int count(const std::string& plugin, const std::string& query);
    std::vector<Title> search(const std::string& plugin, const std::string& query, int limit, int offset);

private:
    std::unique_ptr<StoreImpl> impl_;
};
