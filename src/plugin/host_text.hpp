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
#include <date/date.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace plugin::host_text {

// ---------------------------------------------------------------------------
// normalize_status
// ---------------------------------------------------------------------------

/** \brief Normalize a site-specific status word to the canonical host
 *         vocabulary. Unknown / empty input maps to "unknown". */
inline std::string normalize_status(std::string_view raw) {
    auto lower = [](std::string_view s) {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
        return out;
    };
    const std::string k = lower(raw);
    if (k.empty()) {
        return "unknown";
    }
    static const std::map<std::string, std::string> kMap{
        {"ongoing", "Ongoing"},
        {"releasing", "Ongoing"},
        {"publishing", "Ongoing"},
        {"completed", "Completed"},
        {"finished", "Completed"},
        {"hiatus", "Hiatus"},
        {"on hiatus", "Hiatus"},
        {"dropped", "Dropped"},
        {"cancelled", "Dropped"},
        {"canceled", "Dropped"},
        {"upcoming", "Upcoming"},
        {"not yet released", "Upcoming"},
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
inline std::optional<float> chapter_num(std::string_view s) {
    // Fixed patterns, compiled once (function-local static, thread-safe).
    re2::RE2::Options io;
    io.set_case_sensitive(false);
    static const re2::RE2 kChapterMarker(
        R"((?:\b(?:chapters?|chaps?|chs?|episodes?|eps?)\s*\.?\s*|#\s*)(\d+(?:[.,]\d+)?))", io);
    static const re2::RE2 kVolumeOnly(R"(\b(?:volumes?|vols?|books?)\b)", io);
    static const re2::RE2 kFirstNumber(R"(\d+(?:[.,]\d+)?)");

    const auto parse_decimal = [](std::string_view sv) -> float {
        if (sv.empty()) {
            return 0.0f;
        }
        std::string tmp(sv);
        std::replace(tmp.begin(), tmp.end(), ',', '.');
        try {
            return std::stof(tmp);
        } catch (...) {
            return 0.0f;
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

/// Format a time_point to "YYYY-MM-DD" in UTC using date::format.
inline std::string format_date(std::chrono::system_clock::time_point tp) {
    return date::format("%Y-%m-%d", tp);
}

} // namespace detail

/** \brief Parse a human-readable date (unix timestamp, relative phrase, or a
 *         common layout) to an ISO 8601 date string "YYYY-MM-DD".
 *         Returns std::nullopt when nothing parses. */
inline std::optional<std::string> date_to_iso(std::string_view raw) {
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
                const auto tp = (len >= 12)
                                    ? std::chrono::system_clock::time_point(std::chrono::milliseconds(n))
                                    : std::chrono::system_clock::time_point(std::chrono::seconds(n));
                return detail::format_date(tp);
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
            return detail::format_date(now);
        }
        if (lower == "yesterday") {
            return detail::format_date(now - std::chrono::hours(24));
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
                    return detail::format_date(now - it->second * n);
                }
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    // --- fixed layouts via date::parse ---------------------------------------
    // The layouts the host knows; Go dateLayouts port.
    static const char* kLayouts[] = {
        "%Y-%m-%dT%H:%M:%S",        // RFC3339 (no tz) / ISO 8601
        "%Y-%m-%d %H:%M:%S",        // space separator
        "%Y-%m-%d %H:%M",           // no seconds
        "%Y-%m-%d",                 // date only
        "%d %b %Y",                 // 02 Jan 2006
        "%e %b %Y",                 // 2 Jan 2006
        "%b %e, %Y",              // Jan 2, 2006
        "%b %d, %Y",              // Jan 02, 2006
        "%b %d %Y",               // Jan 02 2006
        "%B %e, %Y",              // January 2, 2006
        "%B %d, %Y",              // January 02, 2006
        "%a, %d %b %Y %H:%M:%S",  // RFC1123: Wed, 02 Jan 2006 15:04:05
        "%a %d %b %y %H:%M",      // RFC822-ish: Wed 02 Jan 06 15:04
    };
    for (const char* fmt : kLayouts) {
        try {
            date::sys_seconds tp{};
            std::istringstream ss(s);
            date::from_stream(ss, fmt, tp);
            if (ss.fail()) {
                continue; // try next layout
            }
            return detail::format_date(tp);
        } catch (...) {
            continue; // date::parse throws on invalid input; try next layout
        }
    }
    return std::nullopt;
}

} // namespace plugin::host_text
