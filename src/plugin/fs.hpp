#pragma once
// plugin::fs_read -- confined, read-only file access for plugin runtimes.
// Resolves `rel` under `dir` lexically; paths escaping the root (.. beyond it,
// absolute paths) and non-regular files yield nullopt. Symlink escape is
// tolerated in v1 (ponytail: realpath audit if a plugin ever needs it).
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace plugin {

inline std::optional<std::string> fs_read(const std::filesystem::path& dir, const std::string& rel) {
    namespace fs = std::filesystem;
    if (rel.empty()) {
        return std::nullopt;
    }
    const fs::path base = fs::path(dir).lexically_normal();
    const fs::path full = (base / fs::path(rel)).lexically_normal();
    const std::string base_s = base.generic_string();
    const std::string full_s = full.generic_string();
    if (full_s.compare(0, base_s.size(), base_s) != 0) {
        return std::nullopt;  // outside the root
    }
    if (full_s.size() > base_s.size() && full_s[base_s.size()] != '/') {
        return std::nullopt;  // prefix collision: plugins/good vs plugins/goodie
    }
    if (full_s == base_s) {
        return std::nullopt;  // the root itself is not a file
    }
    std::error_code ec;
    if (!fs::is_regular_file(full, ec)) {
        return std::nullopt;
    }
    std::ifstream in(full, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace plugin
