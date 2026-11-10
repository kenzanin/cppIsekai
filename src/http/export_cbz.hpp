#pragma once

/** \file export_cbz.hpp
 *  \brief CBZ export core -- no crow, so tests run without a server. */

#include <optional>
#include <string>

namespace cbz {

/** \brief Zip the cached manga pages under
 *   <cache_root>/img/<plugin>/manga/<slug>/<ch-dir>/pNNN.ext ->
 *   <ch-dir>/pNNN.ext (relative, lex order = reading order). Slug may carry a
 *   "/chNNN" suffix exporting a single chapter (prefix-matched, so the id8
 *   collision variant still lands).
 *
 *  Cover omitted: v3 cover files are content-hash named -- no slug mapping at
 *  fs level; resolve store-side and pass a path in when wanted.
 *  Nothing cached (including an unknown slug) yields an empty-but-valid zip;
 *  nullopt only when a file cannot be read or the zip cannot be written. */
std::optional<std::string> build(const std::string& cache_root, const std::string& plugin,
                                 const std::string& slug);

} // namespace cbz
