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

namespace detail {

/// Parse `html` into a lexbor document; returns null on parse failure (lenient).
inline lxb_html_document_t* parse_doc(const std::string& html) {
    lxb_html_document_t* doc = lxb_html_document_create();
    if (doc == nullptr) {
        return nullptr;
    }
    const lxb_status_t st =
        lxb_html_document_parse(doc, reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
    if (st != LXB_STATUS_OK) {
        lxb_html_document_destroy(doc);
        return nullptr;
    }
    return doc;
}

/// Get the text content of the parsed document's `<html>` element.
/// The document root node has no text; the first child is `<html>`.
inline std::string doc_text(lxb_html_document_t* doc, lxb_dom_document_t* ddoc) {
    lxb_dom_node_t* root = lxb_dom_interface_node(doc);
    lxb_dom_node_t* html_el = root->first_child;
    if (html_el == nullptr) {
        return {};
    }
    size_t tl = 0;
    lxb_char_t* text = lxb_dom_node_text_content(html_el, &tl);
    if (text == nullptr) {
        return {};
    }
    const std::string out(reinterpret_cast<const char*>(text), tl);
    lxb_dom_document_destroy_text(ddoc, text);
    return out;
}

// ---------------------------------------------------------------------------
// numeric references (text-entity-fixpoint D3: RE2 match + utf8proc encode)
// ---------------------------------------------------------------------------

/// Lenient numeric reference: optional '#', decimal 1-7 digits or hex 1-6
/// digits. Sites drop the '#' (`&039;`), so it is optional here. Compiled once.
inline const re2::RE2& numeric_ref() {
    static const re2::RE2 kRe(R"(&(?:#)?([0-9]{1,7}|[xX][0-9a-fA-F]{1,6});)");
    return kRe;
}

/// Codepoint a numeric reference may legally encode (HTML: 0, surrogates and
/// > U+10FFFF are invalid).
inline bool valid_cp(int64_t cp) {
    return cp > 0 && cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
}

/// Codepoint of a numeric_ref() capture group (decimal, or hex after x/X).
/// The pattern bounds the digit count, so stoll cannot overflow.
inline int64_t ref_cp(std::string_view digits) {
    const bool hex = digits.size() > 1 && (digits[0] == 'x' || digits[0] == 'X');
    return std::stoll(std::string(hex ? digits.substr(1) : digits), nullptr, hex ? 16 : 10);
}

/// Escape the '&' of numeric references whose codepoint is invalid, so lexbor
/// emits them verbatim instead of U+FFFD (HTML parse-error substitution).
inline std::string escape_invalid_refs(std::string_view s) {
    const re2::RE2& re = numeric_ref();
    std::string out;
    size_t pos = 0;
    size_t copied = 0;
    re2::StringPiece g[2];
    while (pos < s.size()) {
        const std::string_view rest = s.substr(pos);
        if (!re.Match(rest, 0, rest.size(), re2::RE2::UNANCHORED, g, 2)) {
            break;
        }
        const size_t at = static_cast<size_t>(g[0].data() - rest.data());
        if (valid_cp(ref_cp(std::string_view(g[1].data(), g[1].size())))) {
            pos += at + static_cast<size_t>(g[0].size());
            continue;
        }
        out.append(s.substr(copied, pos + at - copied));
        out += "&amp;";
        pos += at + 1; // leave the rest of the reference as plain text
        copied = pos;
    }
    out.append(s.substr(copied));
    return out;
}

/// Decode every valid numeric reference lexbor left alone (`&039;`, `&x27;`)
/// with utf8proc; invalid codepoints stay literal.
inline std::string numeric_sweep(std::string_view s) {
    const re2::RE2& re = numeric_ref();
    std::string out;
    size_t pos = 0;
    size_t copied = 0;
    re2::StringPiece g[2];
    while (pos < s.size()) {
        const std::string_view rest = s.substr(pos);
        if (!re.Match(rest, 0, rest.size(), re2::RE2::UNANCHORED, g, 2)) {
            break;
        }
        const size_t at = static_cast<size_t>(g[0].data() - rest.data());
        const int64_t cp = ref_cp(std::string_view(g[1].data(), g[1].size()));
        const size_t end = pos + at + static_cast<size_t>(g[0].size());
        if (valid_cp(cp)) {
            out.append(s.substr(copied, pos + at - copied));
            utf8proc_uint8_t buf[4];
            const utf8proc_ssize_t n = utf8proc_encode_char(static_cast<utf8proc_int32_t>(cp), buf);
            if (n > 0) {
                out.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
            }
            copied = end;
        }
        pos = end;
    }
    out.append(s.substr(copied));
    return out;
}

/// One decode pass: protect invalid references, lexbor entity decode, sweep.
inline std::string decode_pass(std::string_view in) {
    const std::string s = escape_invalid_refs(in);
    std::string out;
    if (lxb_html_document_t* doc = parse_doc(s); doc != nullptr) {
        lxb_dom_document_t* ddoc = lxb_dom_interface_document(doc);
        out = doc_text(doc, ddoc);
        lxb_html_document_destroy(doc);
    }
    return numeric_sweep(out.empty() ? s : out); // parse miss: pass input verbatim
}

} // namespace detail

/** \brief Decode HTML entities (`&amp;` → `&`, `&lt;` → `<`, numeric refs, etc.)
 *         in `str` via lexbor's lenient HTML parse + text extraction, to a
 *         fixed point (max 4 passes, text-entity-fixpoint D1/D2). Strings with
 *         no '&' are returned untouched without a parse. */

} // namespace plugin::host_text
