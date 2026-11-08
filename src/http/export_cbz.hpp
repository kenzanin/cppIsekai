#pragma once

/** \file export_cbz.hpp
 *  \brief CBZ export core -- no crow, so tests run without a server. */

#include <optional>
#include <string>

namespace cbz {

/** \brief Zip the cached manga tree under <cache_root>/img/<plugin>:
 *   covers/<slug>.<ext>  -> cover.<ext>
 *   <slug>/chNNN/pNNN.ext -> chNNN/pNNN.ext (relative, lex order = reading
 *   order)
 *
 *  Nothing cached (including an unknown slug) yields an empty-but-valid zip;
 *  nullopt only when a file cannot be read or the zip cannot be written. */
std::optional<std::string> build(const std::string& cache_root, const std::string& plugin,
                                 const std::string& slug);

} // namespace cbz
