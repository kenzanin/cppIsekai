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

// ---------------------------------------------------------------------------
// normalize_status
// ---------------------------------------------------------------------------

/** \brief Normalize a site-specific status word to the canonical host
 *         vocabulary. The lookup key is lowercased with every non-alnum
 *        character stripped ("On-Going", "on_going", "On Going" and
 *        "onhold" all collapse), so callers pass raw site values verbatim.
 *        Unknown / empty input maps to "unknown"; recognized-but-unmapped
 *        vocabulary passes through unchanged. */
inline std::string normalize_status(std::string_view raw) {
    std::string k;
    k.reserve(raw.size());
    for (const char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) != 0) {
            k += static_cast<char>(std::tolower(uc));
        }
    }
    if (k.empty()) {
        return "unknown";
    }
    // Union of every plugin copy's synonym set (plugin-host-promotion D0:
    // ongoin/onhaiatus typos are real upstream data, publish*/finish/
    // onhold/notyetpublished come from the mangabay variant).
    static const std::map<std::string, std::string> kMap{
        {"ongoing", "Ongoing"},
        {"ongoin", "Ongoing"},
        {"releasing", "Ongoing"},
        {"publishing", "Ongoing"},
        {"publish", "Ongoing"},
        {"published", "Ongoing"},
        {"completed", "Completed"},
        {"finished", "Completed"},
        {"finish", "Completed"},
        {"hiatus", "Hiatus"},
        {"onhiatus", "Hiatus"},
        {"onhold", "Hiatus"},
        {"dropped", "Dropped"},
        {"cancelled", "Dropped"},
        {"canceled", "Dropped"},
        {"upcoming", "Upcoming"},
        {"notyetreleased", "Upcoming"},
        {"notyetpublished", "Upcoming"},
    };
    if (auto it = kMap.find(k); it != kMap.end()) {
        return it->second;
    }
    return std::string(raw); // pass-through for unrecognized vocabulary
}

// ---------------------------------------------------------------------------
// chapter_num
// ---------------------------------------------------------------------------

/** \brief Extract a chapter number from a title/slug/URL.
 *         "Ch. 12.5" → 12.5, "Vol. 3" → 0 (volume-only → no chapter),
 *         "One Piece 1085" → 1085.0. Returns 0.0f when the string is
 *         volume-only; std::nullopt when no number is present. */
inline std::optional<double> chapter_num(std::string_view s) {
    // Fixed patterns, compiled once (function-local static, thread-safe).
    re2::RE2::Options io;
    io.set_case_sensitive(false);
    static const re2::RE2 kChapterMarker(
        R"((?:\b(?:chapters?|chaps?|chs?|episodes?|eps?)\s*\.?\s*|#\s*)(\d+(?:[.,]\d+)?))", io);
    static const re2::RE2 kVolumeOnly(R"(\b(?:volumes?|vols?|books?)\b)", io);
    static const re2::RE2 kFirstNumber(R"(\d+(?:[.,]\d+)?)");

    const auto parse_decimal = [](std::string_view sv) -> double {
        if (sv.empty()) {
            return 0.0;
        }
        std::string tmp(sv);
        std::replace(tmp.begin(), tmp.end(), ',', '.');
        try {
            return std::stod(tmp);
        } catch (...) {
            return 0.0;
        }
    };

    const std::string text{s};

    // 1) Number attached to a chapter word wins.
    {
        re2::StringPiece groups[2]; // groups[0]=whole match, groups[1]=the number
        if (kChapterMarker.Match(text, 0, text.size(), re2::RE2::UNANCHORED, groups, 2)) {
            const auto& sp = groups[1];
            return parse_decimal(sp.data() != nullptr ? std::string_view(sp.data(), sp.size()) : std::string_view{});
        }
    }

    // 2) Volume-only ("Vol. 3") → 0, because numbering a chapter from a
    //    volume would corrupt reading order.
    {
        re2::StringPiece dummy;
        if (kVolumeOnly.Match(text, 0, text.size(), re2::RE2::UNANCHORED, &dummy, 1)) {
            return 0.0f;
        }
    }

    // 3) Otherwise the first number in the string.
    {
        re2::StringPiece hit;
        if (kFirstNumber.Match(text, 0, text.size(), re2::RE2::UNANCHORED, &hit, 1)) {
            return parse_decimal(std::string_view(hit.data(), hit.size()));
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// date_to_iso
// ---------------------------------------------------------------------------

namespace detail {

/// Format a time_point to "YYYY-MM-DD" in UTC (gmtime_r, no tz dependency).
inline std::string format_date(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char out[16];
    return std::string(out, std::strftime(out, sizeof out, "%Y-%m-%d", &tm));
}

} // namespace detail

/** \brief Parse a human-readable date (unix seconds/millis, relative phrase,
 *         or a common layout) to a UTC time_point. Returns std::nullopt when
 *         nothing parses; date_to_iso / date_epoch format the result. */
inline std::optional<std::chrono::system_clock::time_point> parse_date(std::string_view raw) {
    const auto trim = [](std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) {
            s.remove_suffix(1);
        }
        return s;
    };

    const std::string s = std::string(trim(raw));
    if (s.empty()) {
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now();

    // --- unix seconds / milliseconds ----------------------------------------
    const bool all_digits = std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if (all_digits) {
        const size_t len = s.size();
        if ((len >= 9 && len <= 11) || (len >= 12 && len <= 14)) {
            try {
                const int64_t n = std::stoll(s);
                return (len >= 12) ? std::chrono::system_clock::time_point(std::chrono::milliseconds(n))
                                   : std::chrono::system_clock::time_point(std::chrono::seconds(n));
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    // --- relative phrases ----------------------------------------------------
    {
        std::string lower(s);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (lower == "just now" || lower == "today" || lower == "moments ago") {
            return now;
        }
        if (lower == "yesterday") {
            return now - std::chrono::hours(24);
        }
        re2::RE2::Options rel_opts;
        rel_opts.set_case_sensitive(false);
        static const re2::RE2 kRel(R"((\d+)\s*(minutes?|mins?|hours?|hrs?|days?|weeks?|months?|years?)\s+ago\b)",
                                   rel_opts);
        re2::StringPiece m[3]; // whole, num, unit
        if (kRel.Match(s, 0, s.size(), re2::RE2::UNANCHORED, m, 3)) {
            try {
                const int64_t n = std::stoll(std::string(m[1].data(), m[1].size()));
                std::string unit(m[2].data(), m[2].size());
                std::transform(unit.begin(), unit.end(), unit.begin(), [](unsigned char c) { return std::tolower(c); });
                if (!unit.empty() && unit.back() == 's') {
                    unit.pop_back();
                }
                static const std::map<std::string, std::chrono::minutes> kUnits{
                    {"minute", std::chrono::minutes(1)},
                    {"min", std::chrono::minutes(1)},
                    {"hour", std::chrono::minutes(60)},
                    {"hr", std::chrono::minutes(60)},
                    {"day", std::chrono::minutes(60 * 24)},
                    {"week", std::chrono::minutes(60 * 24 * 7)},
                    {"month", std::chrono::minutes(60 * 24 * 30)},
                    {"year", std::chrono::minutes(60 * 24 * 365)},
                };
                if (const auto it = kUnits.find(unit); it != kUnits.end()) {
                    return now - it->second * n;
                }
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    // --- fixed layouts via strptime -------------------------------------------
    // The layouts the host knows; Go dateLayouts port.
    static const char* kLayouts[] = {
        "%Y-%m-%dT%H:%M:%S",       // RFC3339 (no tz) / ISO 8601
        "%Y-%m-%d %H:%M:%S",       // space separator
        "%Y-%m-%d %H:%M",          // no seconds
        "%Y-%m-%d",                // date only
        "%d %b %Y",                // 02 Jan 2006
        "%e %b %Y",                // 2 Jan 2006
        "%b %e, %Y",               // Jan 2, 2006
        "%b %d, %Y",               // Jan 02, 2006
        "%b %d %Y",                // Jan 02 2006
        "%B %e, %Y",               // January 2, 2006
        "%B %d, %Y",               // January 02, 2006
        "%a, %d %b %Y %H:%M:%S",   // RFC1123: Wed, 02 Jan 2006 15:04:05
        "%a %d %b %y %H:%M",       // RFC822-ish: Wed 02 Jan 06 15:04
    };
    for (const char* fmt : kLayouts) {
        std::tm tm{};
        if (strptime(s.c_str(), fmt, &tm) != nullptr) {
            return std::chrono::system_clock::from_time_t(timegm(&tm));
        }
    }
    return std::nullopt;
}

/** \brief Parse a human-readable date to "YYYY-MM-DD"; nullopt when nothing
 *         parses. */
inline std::optional<std::string> date_to_iso(std::string_view raw) {
    const auto tp = parse_date(raw);
    if (!tp) {
        return std::nullopt;
    }
    return detail::format_date(*tp);
}

/** \brief Parse a human-readable date to unix seconds (UTC); nullopt when
 *         nothing parses. Same parser as date_to_iso. */
inline std::optional<long long> date_epoch(std::string_view raw) {
    const auto tp = parse_date(raw);
    if (!tp) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(tp->time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// trim (P3, utf8proc)
// ---------------------------------------------------------------------------

/** \brief UTF-8-aware trim: strip leading/trailing whitespace and control
 *         codepoints; text is otherwise preserved byte-for-byte (no casefold --
 *         titles/metadata must keep their casing). */
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

} // namespace detail

/** \brief Decode HTML entities (`&amp;` → `&`, `&lt;` → `<`, numeric refs, etc.)
 *         in `str` via lexbor's lenient HTML parse + text extraction. */
inline std::string unescape(std::string_view str) {
    const std::string s{str};
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
    return out.empty() ? s : out;
}

/** \brief Strip all HTML tags from `html`, keeping text content; returns
 *         trimmed text with entities decoded (via lexbor parse + text walk). */
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
