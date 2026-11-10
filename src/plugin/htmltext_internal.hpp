/** \file htmltext_internal.hpp
 *  \brief Internal seam between the re2 and lexbor halves of htmltext. */
#pragma once
#include <string>
namespace plugin {
/** \brief Trimmed copy of `raw` (leading/trailing ASCII whitespace only). */
std::string trim_text(const std::string& raw);
} // namespace plugin
