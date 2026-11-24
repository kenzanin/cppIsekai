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

inline std::string strip_html(std::string_view html) {
    const std::string s{html};
    if (s.empty()) {
        return s;
    }
    lxb_html_document_t* doc = detail::parse_doc(s);
    if (doc == nullptr) {
        return s; // undecodable: verbatim
    }
    lxb_dom_document_t* ddoc = lxb_dom_interface_document(doc);
    const std::string out = detail::doc_text(doc, ddoc);
    lxb_html_document_destroy(doc);
    return trim(out);
}

// ---------------------------------------------------------------------------
// json_blob (plugin-host-promotion D5, goIsekai pluginutil.JSONBlob)
// ---------------------------------------------------------------------------

/** \brief Return the first balanced JSON object/array embedded in `text`,
 *         starting at the first occurrence of `marker` (empty marker searches
 *         from the start). Quotes and escapes are tracked, so a brace inside a
 *         string never closes the blob early. nullopt when no balanced blob
 *         follows the marker. */
inline std::optional<std::string> json_blob(std::string_view text, std::string_view marker) {
    if (!marker.empty()) {
        const size_t at = text.find(marker);
        if (at == std::string_view::npos) {
            return std::nullopt;
        }
        text.remove_prefix(at + marker.size());
    }
    const size_t start = text.find_first_of("{[");
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    const char open = text[start];
    const char closer = open == '[' ? ']' : '}';
    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    for (size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == open) {
            ++depth;
        } else if (c == closer) {
            if (--depth == 0) {
                return std::string(text.substr(start, i - start + 1));
            }
        }
    }
    return std::nullopt;
}


} // namespace plugin::host_text
