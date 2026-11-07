#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "reader/window.hpp"

namespace {

using reader::Chapter;
using reader::PagesOf;
using reader::Position;
using reader::Skipped;

std::vector<std::string> pages(const std::string& ch, int count) {
    std::vector<std::string> v;
    for (int p = 1; p <= count; ++p)
        v.push_back(ch + "/p" + std::to_string(p));
    return v;
}

// Four chapters of five pages each: enough to fill a 10-window after any
// single-chapter remainder, and to prove rerouting past a skipped chapter.
struct Book {
    std::map<std::string, std::vector<std::string>> by_id{
        {"c1", pages("c1", 5)}, {"c2", pages("c2", 5)}, {"c3", pages("c3", 5)}, {"c4", pages("c4", 5)}};
    const std::vector<Chapter> ordered{{"c1", 1}, {"c2", 2}, {"c3", 3}, {"c4", 4}};

    PagesOf of() const {
        return [this](const std::string& id) {
            const auto it = by_id.find(id);
            return it == by_id.end() ? std::vector<std::string>{} : it->second;
        };
    }
};

} // namespace

TEST_CASE("window: boundary spill fills from the next chapter") {
    const Book book;

    // position c1 p5 is that chapter's last page: the whole window spills
    const auto w = reader::next_window(book.ordered, book.of(), Position{"c1", 5}, {});
    const std::vector<std::string> want{"c2/p1", "c2/p2", "c2/p3", "c2/p4", "c2/p5",
                                        "c3/p1", "c3/p2", "c3/p3", "c3/p4", "c3/p5"};
    CHECK(w == want);
}

TEST_CASE("window: no page at or before the position, mid-chapter spill included") {
    const Book book;

    const auto w = reader::next_window(book.ordered, book.of(), Position{"c2", 3}, {});
    CHECK(w.size() == 10); // c2 p4..p5, c3 p1..p5, then c4 p1..p3
    CHECK(w.front() == "c2/p4");
    CHECK(w[2] == "c3/p1");
    CHECK(w.back() == "c4/p3");
    CHECK(std::none_of(w.begin(), w.end(), [](const std::string& u) { return u.find("c1/") == 0; }));
    for (int p = 1; p <= 3; ++p) // nothing at or before the current page
        CHECK(std::find(w.begin(), w.end(), "c2/p" + std::to_string(p)) == w.end());
}

TEST_CASE("window: skipped chapter excluded, window reroutes past it") {
    const Book book;

    const auto w = reader::next_window(book.ordered, book.of(), Position{"c1", 5}, Skipped{{"c2", true}});
    const std::vector<std::string> want{"c3/p1", "c3/p2", "c3/p3", "c3/p4", "c3/p5",
                                        "c4/p1", "c4/p2", "c4/p3", "c4/p4", "c4/p5"};
    CHECK(w == want); // no c2 anywhere: nothing to fetch from a skipped chapter
}

TEST_CASE("window: position edge cases and the limit") {
    const Book book;
    const auto of = book.of();

    // a skipped position chapter does not anchor: the window continues later
    const auto skipped_current = reader::next_window(book.ordered, of, Position{"c1", 5}, Skipped{{"c1", true}});
    CHECK(skipped_current.front() == "c2/p1");
    CHECK(skipped_current.size() == 10);

    // unknown position chapter: no anchor, no window
    CHECK(reader::next_window(book.ordered, of, Position{"nope", 1}, {}).empty());

    // caller order is irrelevant; fractional numbers sort as numbers
    const std::vector<Chapter> messy{{"c3", 3}, {"c1", 1.5}, {"c2", 2}};
    const auto w = reader::next_window(messy, of, Position{"c2", 5}, {});
    CHECK(w.size() == 5);
    CHECK(w.front() == "c3/p1");

    // the window knob is a knob
    CHECK(reader::next_window(book.ordered, of, Position{"c1", 5}, {}, 3).size() == 3);
}
