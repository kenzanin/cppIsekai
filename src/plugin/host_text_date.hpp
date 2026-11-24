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

} // namespace plugin::host_text
