#pragma once

/** \file parse_num.hpp
 *  \brief Error-checked integer parse for untrusted text (query params, state rows).
 */

#include <charconv>
#include <string_view>
#include <system_error>

namespace parse {

/** \brief Integer from text, no throw and no UB on overflow (std::from_chars).
 *  Empty, malformed, trailing junk or out-of-range => \p fallback.
 *  Replaces atoi, which cannot report a conversion error. */
inline int to_int(std::string_view text, int fallback = 0) {
    if (text.empty()) {
        return fallback;
    }
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return (ec == std::errc{} && ptr == text.data() + text.size()) ? value : fallback;
}

} // namespace parse
