#pragma once

/** \file assets.hpp
 *  \brief Optional in-memory asset root. When [assets] path names a .zip or a
 *  folder, templates/static/icon are loaded once at boot and served from RAM;
 *  with an empty path everything keeps reading from the repo layout. */

#include <filesystem>
#include <string>
#include <string_view>

namespace assets {

/** Load `path` (zip file or folder) into memory and install the mustache
 *  loader override. Empty path keeps the on-disk behaviour. */
void init(const std::string& path);

/** Look up a key such as "static/app.css.br" or "templates/detail.mustache".
 *  Returns nullptr in disk mode or on a miss, so callers fall back to disk. */
const std::string* read(std::string_view key);

/** Absolute path for the tray icon. In memory mode the icon bytes are written
 *  once to a temp file (GTK needs a real path); disk mode resolves `fallback`. */
std::filesystem::path icon_path(const std::string& fallback);

/** Remove the temp icon and release the in-memory map. Safe to call twice. */
void shutdown();

bool in_memory();

} // namespace assets
