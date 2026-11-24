/** \file host_text.hpp
 *  \brief host.text.* primitives: status normalization, chapter-number
 *         extraction, and date-to-ISO conversion shared by every plugin
 *         runtime (one core, three thin marshals — design D4).
 *
 *  Ported from goIsekai internal/pluginutil (status.go, extract.go,
 *  dates.go). Pure and site-agnostic; the host is the single place that
 *  owns these transforms so 75+ plugin sites stop re-deriving them.
 */
#pragma once

#include <re2/re2.h>
#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>
#include <utf8proc.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace plugin::host_text {

inline std::string trim(std::string_view raw) {
    const auto is_trim_cp = [](utf8proc_int32_t cp) {
        return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == 0x00A0 ||
               (cp >= 0x2000 && cp <= 0x200F) || cp == 0x2028 || cp == 0x2029 || cp == 0x3000 ||
               cp == 0xFEFF || cp < 0x20;
    };
    const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(raw.data());
    utf8proc_int32_t cp = 0;
    size_t b = 0;
    while (b < raw.size()) {
        const auto n = utf8proc_iterate(bytes + b, static_cast<utf8proc_ssize_t>(raw.size() - b), &cp);
        if (n <= 0 || !is_trim_cp(cp)) {
            break;
        }
        b += static_cast<size_t>(n);
    }
    size_t e = raw.size();
    while (e > b) {
        size_t start = e - 1;
        while (start > b && (raw[start] & 0xC0) == 0x80) {
            --start;
        }
        const auto n = utf8proc_iterate(bytes + start, static_cast<utf8proc_ssize_t>(e - start), &cp);
        if (n <= 0 || static_cast<size_t>(n) != e - start || !is_trim_cp(cp)) {
            break;
        }
        e = start;
    }
    return std::string(raw.substr(b, e - b));
}

// ---------------------------------------------------------------------------
// unescape + strip_html (P3, lexbor)
// ---------------------------------------------------------------------------

inline std::string unescape(std::string_view str) {
    if (str.find('&') == std::string_view::npos) {
        return std::string(str); // no entity marker: no parse at all
    }
    std::string s{str};
    for (int pass = 0; pass < 4 && s.find('&') != std::string::npos; ++pass) {
        std::string next = detail::decode_pass(s);
        if (next == s) {
            break;
        }
        s = std::move(next);
    }
    return s;
}

/** \brief Display-text helper: `trim(unescape(s))` -- decode HTML entities to
 *         a fixed point then trim. Use on scraped titles/descriptions only;
 *         never on URLs, ids or cache keys (text-entity-fixpoint D4/D5). */
inline std::string clean(std::string_view s) { return trim(unescape(s)); }

/** \brief Strip all HTML tags from `html`, keeping text content; returns
 *         trimmed text with entities decoded (via lexbor parse + text walk). */

} // namespace plugin::host_text
