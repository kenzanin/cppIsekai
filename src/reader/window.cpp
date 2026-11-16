#include "reader/window.hpp"

#include <algorithm>

namespace reader {

std::vector<WindowPart> window_parts(const std::vector<Chapter>& chapters, const PagesOf& pages_of, const Position& pos,
                                     const Skipped& skipped, const std::size_t limit) {
    std::vector<WindowPart> parts;
    if (!pages_of) {
        return parts;
    }

    std::vector<Chapter> ordered = chapters; // never trust the caller's order
    std::ranges::stable_sort(ordered, [](const Chapter& a, const Chapter& b) { return a.number < b.number; });

    std::size_t at = ordered.size(); // past-the-end: position chapter unknown -> empty window
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (ordered[i].id == pos.chapter_id) {
            at = i;
            break;
        }
    }

    std::size_t total = 0;
    for (std::size_t i = at; i < ordered.size() && total < limit; ++i) {
        const Chapter& ch = ordered[i];
        const auto flag = skipped.find(ch.id);
        if (flag != skipped.end() && flag->second) {
            continue; // skipped: no pages, keep walking
        }

        const std::vector<std::string> pages = pages_of(ch.id);
        // the position chapter contributes only pages after the current one
        const std::size_t begin = (i == at && pos.page > 0) ? std::min<std::size_t>(pos.page, pages.size()) : 0;
        if (begin >= pages.size()) {
            continue;
        }
        WindowPart part{.chapter_id = ch.id, .number = ch.number, .first_page = static_cast<int>(begin) + 1, .urls = {}};
        for (std::size_t p = begin; p < pages.size() && total < limit; ++p) {
            part.urls.push_back(pages[p]);
            ++total;
        }
        parts.push_back(std::move(part));
    }
    return parts;
}

std::vector<std::string> next_window(const std::vector<Chapter>& chapters, const PagesOf& pages_of, const Position& pos,
                                     const Skipped& skipped, const std::size_t limit) {
    std::vector<std::string> out;
    for (const auto& part : window_parts(chapters, pages_of, pos, skipped, limit)) {
        out.insert(out.end(), part.urls.begin(), part.urls.end());
    }
    return out;
}

} // namespace reader
