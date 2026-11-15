#include "reader/window.hpp"

#include <algorithm>

namespace reader {

std::vector<std::string> next_window(const std::vector<Chapter>& chapters, const PagesOf& pages_of, const Position& pos,
                                     const Skipped& skipped, const std::size_t limit) {
    std::vector<std::string> out;
    if (!pages_of)
        return out;

    std::vector<Chapter> ordered = chapters; // never trust the caller's order
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Chapter& a, const Chapter& b) { return a.number < b.number; });

    std::size_t at = ordered.size(); // past-the-end: position chapter unknown -> empty window
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (ordered[i].id == pos.chapter_id) {
            at = i;
            break;
        }
    }

    for (std::size_t i = at; i < ordered.size() && out.size() < limit; ++i) {
        const auto flag = skipped.find(ordered[i].id);
        if (flag != skipped.end() && flag->second)
            continue; // skipped: no pages, keep walking

        const std::vector<std::string> pages = pages_of(ordered[i].id);
        // the position chapter contributes only pages after the current one
        const std::size_t begin = (i == at && pos.page > 0) ? static_cast<std::size_t>(pos.page) : 0;
        for (std::size_t p = std::min(begin, pages.size()); p < pages.size() && out.size() < limit; ++p)
            out.push_back(pages[p]);
    }
    return out;
}

} // namespace reader
