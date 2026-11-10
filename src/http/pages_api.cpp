#include "pages_api.hpp"

#include "stale.hpp"

#include <crow.h>

#include <string>
#include <variant>
#include <vector>

namespace {

// Percent-encode a query-string value: keep RFC 3986 unreserved chars, encode
// everything else (a page url can carry ?&= itself).
std::string url_encode(const std::string& s) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

} // namespace

std::optional<std::string> build_pages_response(const std::string& chs_env, const std::string& pages_env,
                                                const PagesCtx& ctx) {
    // Resolve the target chapter id + prev/next numbers from the ordered list.
    // Chapter list is number-ascending: last strictly-less = closest prev,
    // first strictly-greater = closest next.
    std::string target_id;
    double prev = 0, next = 0;
    const auto fmt_ch = [](double v) {
        const long long t = static_cast<long long>(v);
        if (v == static_cast<double>(t)) return std::to_string(t); // integral: "2"
        std::string s = std::to_string(v);                          // "1.500000"
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    };
    bool have_prev = false, have_next = false;
    if (const auto chs = envelope_data(chs_env)) {
        for (const auto& item : *chs) {
            if (!item.is_object())
                continue;
            const auto& entry = item.get_object();
            const auto num = entry.find("number");
            if (num == entry.end())
                continue;
            const auto* d = std::get_if<double>(&num->second.data);
            if (!d)
                continue;
            if (*d == ctx.ch && target_id.empty()) {
                const auto idf = entry.find("id");
                if (idf != entry.end()) {
                    if (const auto* s = std::get_if<std::string>(&idf->second.data))
                        target_id = *s;
                }
            } else if (*d < ctx.ch) {
                prev = *d;
                have_prev = true;
            } else if (*d > ctx.ch && !have_next) {
                next = *d;
                have_next = true;
            }
        }
    }
    if (target_id.empty())
        return std::nullopt;

    const auto pages = envelope_data(pages_env);
    if (!pages || pages->empty())
        return std::nullopt;

    crow::json::wvalue out;
    out["plugin"] = ctx.plugin;
    out["slug"] = ctx.slug;
    out["chapter"] = ctx.ch;
    std::vector<crow::json::wvalue> page_urls;
    int page = 0;
    for (const auto& el : *pages) {
        if (const auto* s = std::get_if<std::string>(&el.data)) {
            ++page; // 1-based page number, index stays 0-based
            page_urls.emplace_back("/cache/img?url=" + url_encode(*s) + "&plugin=" + url_encode(ctx.plugin) +
                                   "&slug=" + url_encode(ctx.slug) + "&ch=" + fmt_ch(ctx.ch) +
                                   "&page=" + std::to_string(page));
        }
    }
    out["pages"] = std::move(page_urls);
    if (have_prev)
        out["prev_ch"] = prev;
    if (have_next)
        out["next_ch"] = next;
    return out.dump();
}
