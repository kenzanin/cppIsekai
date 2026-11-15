#include "pages_api.hpp"

#include "stale.hpp"

#include <crow.h>

#include <algorithm>
#include <string>
#include <utility>
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
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
            c == '_' || c == '~') {
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
    // Plugin order is not trusted (mangafreak emits newest-first), so the
    // list is stable-sorted by number once and prev/next are the entries
    // sitting next to the target (design D3). Same-number entries become
    // consecutive stops, which is what makes duplicate releases individually
    // reachable. The target is `ctx.ch_id` when set, else the first entry
    // whose number is `ctx.ch` (legacy, design D1).
    std::vector<std::pair<double, std::string>> order;
    if (const auto chs = envelope_data(chs_env)) {
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
            if (d == nullptr) {
                continue;
            }
            std::string id;
            const auto* const idf = entry.find("id");
            if (idf != entry.end()) {
                if (const auto* s = std::get_if<std::string>(&idf->second.data)) {
                    id = *s;
                }
            }
            order.emplace_back(*d, std::move(id));
        }
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    size_t target_idx = order.size();
    if (!ctx.ch_id.empty()) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].second == ctx.ch_id) {
                target_idx = i;
                break;
            }
        }
    }
    if (target_idx == order.size()) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].first == ctx.ch) {
                target_idx = i;
                break;
            }
        }
    }
    if (target_idx == order.size()) {
        return std::nullopt;
    }
    const std::string target_id = order[target_idx].second;
    if (target_id.empty()) {
        return std::nullopt; // the plugin's pages call needs the entry's id
    }
    const auto fmt_ch = [](double v) {
        const auto t = static_cast<long long>(v);
        if (v == static_cast<double>(t)) {
            return std::to_string(t); // integral: "2"
        }
        std::string s = std::to_string(v); // "1.500000"
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') {
            s.pop_back();
        }
        return s;
    };
    const bool have_prev = target_idx > 0;
    const bool have_next = target_idx + 1 < order.size();

    const auto pages = envelope_data(pages_env);
    if (!pages || pages->empty()) {
        return std::nullopt;
    }

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
                                   "&page=" + std::to_string(page) + "&ch_id=" + url_encode(target_id));
        }
    }
    out["pages"] = std::move(page_urls);
    out["ch_id"] = target_id; // lets the reader keep the exact release without re-deriving it
    if (have_prev) {
        out["prev_ch"] = order[target_idx - 1].first;
        if (!order[target_idx - 1].second.empty()) {
            out["prev_ch_id"] = order[target_idx - 1].second;
        }
    }
    if (have_next) {
        out["next_ch"] = order[target_idx + 1].first;
        if (!order[target_idx + 1].second.empty()) {
            out["next_ch_id"] = order[target_idx + 1].second;
        }
    }
    return out.dump();
}
