#include "assets.hpp"

#include <crow/mustache.h>
#include <libzippp/libzippp.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>

namespace assets {
namespace {

std::unordered_map<std::string, std::string> g_files;
std::string g_icon_temp;
std::function<std::string(std::string)> g_prev_loader;
bool g_loader_installed = false;

constexpr std::size_t kMaxBytes = 32u * 1024u * 1024u; // whole asset root ceiling
constexpr std::size_t kMaxFile = 8u * 1024u * 1024u;   // mermaid ~5.5 MB, headroom

bool wanted(std::string_view key) {
    return key.starts_with("templates/") || key.starts_with("static/") ||
           key.starts_with("icon/") || key == "assets/tray.png";
}

void add(std::string key, std::string bytes) {
    if (!wanted(key) || bytes.size() > kMaxFile) {
        return;
    }
    std::size_t total = bytes.size();
    for (const auto& [k, v] : g_files) {
        total += v.size();
    }
    if (total > kMaxBytes) {
        spdlog::warn("[assets] root exceeds {} MB, skipping {}", kMaxBytes / (1024 * 1024), key);
        return;
    }
    g_files.emplace(std::move(key), std::move(bytes));
}

void load_folder(const std::filesystem::path& root) {
    std::error_code ec;
    for (const auto& e : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec || !e.is_regular_file(ec)) {
            continue;
        }
        const auto rel = std::filesystem::relative(e.path(), root, ec).generic_string();
        if (ec || !wanted(rel)) {
            continue;
        }
        std::ifstream f(e.path(), std::ios::binary);
        if (!f) {
            continue;
        }
        add(rel, std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
    }
}

void load_zip(const std::filesystem::path& file) {
    libzippp::ZipArchive archive(file.string());
    if (!archive.open()) {
        spdlog::warn("[assets] cannot open asset archive {}", file.string());
        return;
    }
    for (const auto& entry : archive.getEntries()) {
        if (!entry.isFile()) {
            continue;
        }
        const std::string name = entry.getName();
        if (!wanted(name) || entry.getSize() > kMaxFile) {
            continue;
        }
        add(name, entry.readAsText()); // std::string holds binary bytes too
    }
    archive.close();
}

} // namespace

void init(const std::string& path) {
    if (path.empty()) {
        return; // disk mode: no overrides, no loader hook
    }
    std::error_code ec;
    const std::filesystem::path root(path);
    if (std::filesystem::is_directory(root, ec)) {
        load_folder(root);
    } else if (std::filesystem::is_regular_file(root, ec)) {
        load_zip(root);
    } else {
        spdlog::warn("[assets] path '{}' is neither a folder nor a file; using disk layout", path);
        return;
    }
    if (g_files.empty()) {
        spdlog::warn("[assets] no templates/static/icon entries found in '{}'; using disk layout", path);
        return;
    }

    // Crow resolves partials through the same loader, so overriding it covers
    // both top-level templates and {{>partial}} includes without a temp dir.
    g_prev_loader = crow::mustache::detail::get_loader_ref();
    crow::mustache::detail::get_loader_ref() = [](std::string name) -> std::string {
        if (const auto* bytes = assets::read("templates/" + name)) {
            return *bytes;
        }
        return g_prev_loader ? g_prev_loader(name) : std::string{};
    };
    g_loader_installed = true;
    spdlog::info("[assets] loaded {} entries from {} into memory", g_files.size(), path);
}

const std::string* read(std::string_view key) {
    if (g_files.empty()) {
        return nullptr;
    }
    const auto it = g_files.find(std::string(key));
    return it == g_files.end() ? nullptr : &it->second;
}

std::filesystem::path icon_path(const std::string& fallback) {
    std::error_code ec;
    if (g_files.empty()) {
        const auto abs = std::filesystem::absolute(fallback, ec);
        return ec ? std::filesystem::path(fallback) : abs;
    }
    if (!g_icon_temp.empty() && std::filesystem::exists(g_icon_temp, ec)) {
        return g_icon_temp;
    }
    auto it = g_files.find("icon/tray.png");
    if (it == g_files.end()) {
        it = g_files.find("assets/tray.png"); // pack-assets keeps the repo path
    }
    if (it == g_files.end()) {
        const auto abs = std::filesystem::absolute(fallback, ec);
        return ec ? std::filesystem::path(fallback) : abs;
    }
    const char* tmpl = "/tmp/cppisekai-tray-XXXXXX.png";
    std::string buf = tmpl;
    std::vector<char> path(buf.begin(), buf.end());
    path.push_back('\0');
    const int fd = mkstemps(path.data(), 4);
    if (fd < 0) {
        return std::filesystem::absolute(fallback, ec);
    }
    const std::string tmp(path.data());
    const auto out = std::fopen(tmp.c_str(), "wb"); // reopen through FILE* for simple write
    ::close(fd);
    if (!out) {
        return std::filesystem::absolute(fallback, ec);
    }
    std::fwrite(it->second.data(), 1, it->second.size(), out);
    std::fclose(out);
    g_icon_temp = tmp;
    return g_icon_temp;
}

void shutdown() {
    if (g_loader_installed) {
        crow::mustache::detail::get_loader_ref() = g_prev_loader;
        g_loader_installed = false;
    }
    if (!g_icon_temp.empty()) {
        std::error_code ec;
        std::filesystem::remove(g_icon_temp, ec);
        g_icon_temp.clear();
    }
    g_files.clear();
}

bool in_memory() {
    return !g_files.empty();
}

} // namespace assets
