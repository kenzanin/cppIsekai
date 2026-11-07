#pragma once
// Sliding prefetch window math (image-cache design 5) -- PURE: no I/O here, the
// page lookup is injected. The worker (task 3.3) diffs the result on disk.
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace reader {

struct Chapter {
    std::string id;  // plugin chapter uuid
    double number{}; // sort key; fractional chapter numbers exist (extras)
};

struct Position {
    std::string chapter_id;
    int page{}; // 1-based; the window starts after it
};

// chapter id -> its page image urls, in order; empty when unknown or failed.
using PagesOf = std::function<std::vector<std::string>(const std::string&)>;

// chapter id -> skipped flag; absent means not skipped.
using Skipped = std::map<std::string, bool>;

// Chapters ordered by number (input order kept for ties), skipped ones
// excluded, pages strictly after the position, spilling into later chapters,
// at most `limit` urls. Unknown position chapter -> empty window; a skipped
// position chapter lets the window continue with the next one.
std::vector<std::string> next_window(const std::vector<Chapter>& chapters, const PagesOf& pages_of, const Position& pos,
                                     const Skipped& skipped, std::size_t limit = 10);

} // namespace reader
