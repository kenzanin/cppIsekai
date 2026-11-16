#pragma once

/** \file window.hpp
 *  \brief Sliding prefetch window math (image-cache design 5) -- PURE: no
 *  I/O here, the page lookup is injected. The worker (task 3.3) diffs the
 *  result on disk. */
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

/** \brief One chapter's contiguous slice of a window: the chapter, the
 *  1-based page its first url corresponds to, and the urls. The prefetch
 *  worker lands each part in that chapter's own dir -- a spilled page must
 *  not masquerade as chapter N page N+1 (the cached>total counter bug).
 *  first_page mirrors the /cache/img query-page convention so both paths
 *  land one file per slot. */
struct WindowPart {
    std::string chapter_id;
    double number{};  // chapter number; int part is the dir key
    int first_page{}; // 1-based chapter page of urls[0]; 0 = empty part
    std::vector<std::string> urls;
};

/** \brief Chapters ordered by number (input order kept for ties), skipped
 *  ones excluded, pages strictly after the position, spilling into later
 *  chapters, at most \p limit urls -- as per-chapter parts. Unknown position
 *  chapter -> empty; a skipped position chapter lets the window continue. */
std::vector<WindowPart> window_parts(const std::vector<Chapter>& chapters, const PagesOf& pages_of,
                                     const Position& pos, const Skipped& skipped, std::size_t limit = 10);

/** \brief Flat view of window_parts: the reader response payload. */
std::vector<std::string> next_window(const std::vector<Chapter>& chapters, const PagesOf& pages_of, const Position& pos,
                                     const Skipped& skipped, std::size_t limit = 10);

} // namespace reader
