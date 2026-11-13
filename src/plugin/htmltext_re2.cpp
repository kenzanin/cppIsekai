#include "htmltext.hpp"
#include "htmltext_internal.hpp"

#include <re2/re2.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace plugin {

// ---------------------------------------------------------------------------
// re2 (linear-time; no ReDoS from plugin-authored patterns)
// ---------------------------------------------------------------------------

namespace {

/// Whole match + capture groups for one RE2 call. `full` anchors both ends
/// (RE2 ANCHOR_BOTH) vs first-found (UNANCHORED).
std::vector<std::string> re2_groups(const re2::RE2& re, const std::string& text, bool full) {
    std::vector<std::string> out;
    const int n = re.NumberOfCapturingGroups();
    std::vector<re2::StringPiece> m(static_cast<size_t>(n) + 1);
    const bool hit =
        re.Match(text, 0, text.size(), full ? re2::RE2::ANCHOR_BOTH : re2::RE2::UNANCHORED, m.data(), n + 1);
    if (!hit) {
        return out;
    }
    out.reserve(static_cast<size_t>(n) + 1);
    for (int i = 0; i <= n; ++i) {
        // data()==nullptr: group did not participate (optional group) -> empty
        const auto& sp = m[static_cast<size_t>(i)];
        out.push_back(sp.data() != nullptr ? std::string(sp.data(), sp.size()) : std::string());
    }
    return out;
}

} // namespace

/// Trimmed copy of `raw` (leading/trailing ASCII whitespace only).
std::string trim_text(const std::string& raw) {
    const auto issp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; };
    size_t b = 0, e = raw.size();
    while (b < e && issp(static_cast<unsigned char>(raw[b]))) {
        ++b;
    }
    while (e > b && issp(static_cast<unsigned char>(raw[e - 1]))) {
        --e;
    }
    return raw.substr(b, e - b);
}

std::vector<std::string> re2_find(const std::string& pattern, const std::string& text) {
    const re2::RE2 re(pattern);
    if (!re.ok()) {
        throw std::invalid_argument("re2: " + re.error());
    }
    return re2_groups(re, text, /*full=*/false);
}

std::vector<std::string> re2_match(const std::string& pattern, const std::string& text) {
    const re2::RE2 re(pattern);
    if (!re.ok()) {
        throw std::invalid_argument("re2: " + re.error());
    }
    return re2_groups(re, text, /*full=*/true);
}

// ---------------------------------------------------------------------------
// P4: find_all, replace, quote, find_index (core, single wrap/marshal rule)
// ---------------------------------------------------------------------------

std::vector<std::vector<std::string>> re2_find_all(const std::string& pattern, const std::string& text) {
    const re2::RE2 re(pattern);
    if (!re.ok()) {
        throw std::invalid_argument("re2: " + re.error());
    }
    std::vector<std::vector<std::string>> out;
    if (text.empty()) {
        return out;
    }
    const int n = re.NumberOfCapturingGroups();
    const int total = n + 1;
    std::vector<re2::StringPiece> m(static_cast<size_t>(total));
    // RE2::Match returns the match length; 0 means end, -1 means no more.
    // We iterate via position advancing, using UNANCHORED search from each offset.
    size_t pos = 0;
    while (pos <= text.size()) {
        const bool hit =
            re.Match(text, static_cast<int>(pos), static_cast<int>(text.size()), re2::RE2::UNANCHORED, m.data(), total);
        if (!hit) {
            break;
        }
        // Match end = match start + len; advancing from `pos` alone would
        // re-find the same match when RE2 started it after `pos`.
        const size_t match_end = static_cast<size_t>(m[0].data() - text.data()) + m[0].size();
        auto groups = std::vector<std::string>(total);
        for (int i = 0; i < total; ++i) {
            const auto& sp = m[static_cast<size_t>(i)];
            if (sp.data() != nullptr) {
                groups[static_cast<size_t>(i)] = std::string(sp.data(), sp.size());
            }
        }
        out.push_back(std::move(groups));
        // Advance past this match; guard against zero-length matches.
        pos = (match_end > pos) ? match_end : pos + 1;
    }
    return out;
}

// Go-style rewrite (goIsekai parity) → RE2 rewrite: $1 / ${1} → \1,
// existing backslashes escaped (RE2 treats \ as special).
// ponytail: single-digit groups only; upgrade to full Go Expand ($name) if a plugin needs it.
static std::string go_repl_to_re2(const std::string& repl) {
    std::string out;
    out.reserve(repl.size() + 4);
    for (size_t i = 0; i < repl.size(); ++i) {
        const char c = repl[i];
        if (c == '\\') {
            out += "\\\\";
            continue;
        }
        if (c == '$' && i + 1 < repl.size()) {
            if (repl[i + 1] == '{') {
                size_t j = i + 2;
                std::string num;
                while (j < repl.size() && repl[j] >= '0' && repl[j] <= '9') {
                    num += repl[j++];
                }
                if (!num.empty() && j < repl.size() && repl[j] == '}') {
                    out += '\\';
                    out += num;
                    i = j;
                    continue;
                }
            } else if (repl[i + 1] >= '0' && repl[i + 1] <= '9') {
                out += '\\';
                out += repl[i + 1];
                ++i;
                continue;
            }
        }
        out += c;
    }
    return out;
}

std::string re2_replace(const std::string& pattern, const std::string& rewrite, const std::string& text) {
    const re2::RE2 re(pattern);
    if (!re.ok()) {
        throw std::invalid_argument("re2: " + re.error());
    }
    std::string out = text;
    re2::RE2::GlobalReplace(&out, re, go_repl_to_re2(rewrite));
    return out;
}

std::string re2_quote(const std::string& s) {
    return re2::RE2::QuoteMeta(s);
}

size_t re2_find_index(const std::string& pattern, const std::string& text) {
    const re2::RE2 re(pattern);
    if (!re.ok()) {
        throw std::invalid_argument("re2: " + re.error());
    }
    // RE2::Match with a single StringPiece out param: the first slot is the
    // whole match. We can use it to get the start offset via the position.
    // Simpler: iterate from each offset until the first UNANCHORED match.
    // RE2::Match returns true and sets m[0] to the match; the match start
    // is at the position we searched from (since UNANCHORED finds the leftmost
    // match at or after `pos`).
    std::vector<re2::StringPiece> m(1);
    // UNANCHORED Match finds the leftmost match at-or-after 0 on the first
    // call, so one shot is enough: 1-based offset, 0 = no match.
    if (!re.Match(text, 0, text.size(), re2::RE2::UNANCHORED, m.data(), 1)) {
        return 0;
    }
    const auto start = static_cast<const char*>(m[0].data()) - text.data();
    return static_cast<size_t>(start) + 1;
}

} // namespace plugin
