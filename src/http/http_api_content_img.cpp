#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace http::api {

// Percent-decode a query value (crow's param() does this internally; the
// raw_url path below must match or curl receives a literal "%3A%2F" host).
std::string pct_decode(const std::string& s) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && nib(s[i + 1]) >= 0 && nib(s[i + 2]) >= 0) {
            out += static_cast<char>(nib(s[i + 1]) * 16 + nib(s[i + 2]));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

// Duplicate-number releases can fail per-entry upstream (comico's CF origin
// 502s only some releases' open call). Siblings of the primary id with the
// same number, id-ascending, primary excluded, capped at three -- the pages
// route falls back across them when the primary yields no pages.
std::vector<std::string> sibling_ids(const std::string& chs_env, double chd, const std::string& primary) {
    std::vector<std::string> out;
    const auto chs = envelope_data(chs_env);
    if (!chs) {
        return out;
    }
    for (const auto& item : *chs) {
        if (!item.is_object()) {
            continue;
        }
        const auto& entry = item.get_object();
        const auto* const num = entry.find("number");
        if (num == entry.end()) {
            continue;
        }
        const auto* d = std::get_if<double>(&num->second.data);
        if (!d || *d != chd) {
            continue;
        }
        const auto* const idf = entry.find("id");
        if (idf == entry.end()) {
            continue;
        }
        const auto* s = std::get_if<std::string>(&idf->second.data);
        if (!s || s->empty() || *s == primary) {
            continue;
        }
        const auto at = std::lower_bound(out.begin(), out.end(), *s);
        if (at == out.end() || *at != *s) {
            out.insert(at, *s);
            if (out.size() > 3) {
                out.pop_back();
            }
        }
    }
    return out;
}

} // namespace http::api
