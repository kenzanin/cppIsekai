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


} // namespace plugin::host_text
