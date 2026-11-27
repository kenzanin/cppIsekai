#include "pages_source.hpp"

#include "pages_api.hpp"
#include "stale.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace http::api {

std::string envelope_error(const std::string& env) {
    glz::generic g;
    if (glz::read_json(g, env) || !g.is_object()) {
        return {};
    }
    const auto& obj = g.get_object();
    const auto it = obj.find("error");
    if (it == obj.end()) {
        return {};
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data)) {
        return *s;
    }
    return {};
}

std::string resolve_chapter_id(const std::string& chs_env, const std::string& wanted, double chd) {
    const auto chs = envelope_data(chs_env);
    if (!chs) {
        return {};
    }
    const auto entry_id = [](const auto& item) -> std::string {
        if (!item.is_object()) {
            return {};
        }
        const auto& entry = item.get_object();
        const auto* const idf = entry.find("id");
        if (idf == entry.end()) {
            return {};
        }
        if (const auto* s = std::get_if<std::string>(&idf->second.data)) {
            return *s;
        }
        return {};
    };
    if (!wanted.empty()) {
        for (const auto& item : *chs) {
            if (entry_id(item) == wanted) {
                return wanted;
            }
        }
    }
    // Number match with ties going to the id-min duplicate: pages_api sorts
    // equal numbers by id, so the neighbor chain starts at the id-min entry --
    // the fallback must land on that same release, not the raw-first one
    // (mangafreak emits `_24b` before `_24a`).
    std::string best;
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
        const std::string id = entry_id(item);
        if (!id.empty() && (best.empty() || id < best)) {
            best = id;
        }
    }
    return best;
}

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

std::vector<std::string> pages_urls(const std::string& env) {
    std::vector<std::string> urls;
    if (const auto data = envelope_data(env)) {
        urls.reserve(data->size());
        for (const auto& el : *data) {
            if (const auto* s = std::get_if<std::string>(&el.data)) {
                urls.emplace_back(*s);
            }
        }
    }
    return urls;
}

PagesSource pick_pages_source(const std::string& chapters_env, double chapter_number, const std::string& wanted,
                              const std::function<std::vector<std::string>(const std::string&)>& stored,
                              const std::function<std::string(const std::string&)>& parse) {
    const std::string primary = resolve_chapter_id(chapters_env, wanted, chapter_number);
    if (primary.empty()) {
        return {};
    }
    std::vector<std::string> candidates = sibling_ids(chapters_env, chapter_number, primary);
    candidates.insert(candidates.begin(), primary);
    for (const auto& cand : candidates) {
        // pages-local-first D1/D2: rows answer without the plugin, and without a
        // 100%-on-disk gate (page cache files are keyed by layout path, not URL).
        auto rows = stored(cand);
        if (!rows.empty()) {
            return PagesSource{.id = cand, .envelope = pages_envelope_from_urls(rows), .from_rows = true};
        }
        std::string env = parse(cand);
        if (!pages_urls(env).empty()) {
            return PagesSource{.id = cand, .envelope = std::move(env), .from_rows = false};
        }
    }
    return {};
}

} // namespace http::api
