#pragma once
// slug — manga title -> filesystem-safe lowercase folder name (design D1).
// Pure/header-only: no crow, no cache.hpp, no DB. Collision rule per spec
// "Slug uniqueness without a DB": suffix `-<last 8 chars of id>` only when a
// second id lands on the same base slug; a lone manga is never suffixed.
#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace cache {

inline constexpr std::size_t slug_max = 64;

// Longest prefix <= max that ends on a UTF-8 character boundary.
inline std::size_t capped_len(std::string_view s, std::size_t max) {
    std::size_t n = std::min(s.size(), max);
    while (n > 0 && n < s.size() && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80)
        --n;
    return n;
}

// Base slug: [a-z0-9] plus verbatim non-ASCII UTF-8 (keeps CJK titles unique;
// bytes >= 0x80 are fs-safe), runs of anything else collapsed to one '-',
// trimmed. Empty/blank title -> "untitled".
inline std::string slug_base(std::string_view title) {
    std::string s;
    s.reserve(title.size());
    bool sep = true; // swallows leading separators
    for (char ch : title) {
        const auto c = static_cast<unsigned char>(ch);
        if (c >= 0x80) {
            s.push_back(ch);
            sep = false;
        } else if (std::isalnum(c)) {
            s.push_back(static_cast<char>(std::tolower(c)));
            sep = false;
        } else if (!sep) {
            s.push_back('-');
            sep = true;
        }
    }
    while (!s.empty() && s.back() == '-')
        s.pop_back();
    s.resize(capped_len(s, slug_max));
    return s.empty() ? std::string("untitled") : s;
}

// Slug for `id` within the known manga set `titles` (id -> title): the base
// slug, plus the id suffix only on a base-slug collision with another id.
inline std::string manga_slug(const std::map<std::string, std::string>& titles, const std::string& id) {
    const auto self = titles.find(id);
    if (self == titles.end())
        return {};
    const std::string base = slug_base(self->second);
    bool collide = false;
    for (const auto& [oid, otitle] : titles)
        if (oid != id && slug_base(otitle) == base) {
            collide = true;
            break;
        }
    if (!collide || id.empty())
        return base;
    const std::string tail = id.size() <= 8 ? id : id.substr(id.size() - 8);
    // ponytail: two same-titled ids sharing their last 8 chars still merge;
    // upgrade path: lengthen the tail to the full id.
    std::string slug = base.substr(0, capped_len(base, slug_max - tail.size() - 1));
    slug += '-';
    slug += tail;
    return slug;
}

} // namespace cache
