#include "htmltext.hpp"
#include "htmltext_internal.hpp"

#include <re2/re2.h>

#include <cstring>
#include <memory>
#include <stdexcept>
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

} // namespace plugin
