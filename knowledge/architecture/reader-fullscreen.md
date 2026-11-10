---
type: decision
title: "reader-fullscreen: full-screen reader chrome + zoom persistence (implemented, uncommitted)"
description: "Reader at /read is a self-contained full-viewport shell (no app chrome). Center-third tap toggles navShown overlay bar; top bar (back + chapter/page selects), bottom bar (mode/dir/zoom + page jump); Done overlay at end of last chapter; zoom persisted per manga (additive reader_positions.zoom column). Keyboard/wheel hardened 2026-11-10. Implemented on main, NOT committed; live smoke partial."
tags: [reader-fullscreen, reader, zoom, openspec, navigation]
generated: { by: agent/mcp, at: "2026-11-10T03:17:47Z" }
status: stable
---

## State
reader-fullscreen implemented on main (uncommitted as of 2026-11-10). Build clean, ctest 14/14. openspec/changes/reader-fullscreen/tasks.md 18/18 checked. Live smoke in blueprint Chrome was partial: confirmed full-screen shell loads, center-third tap click detected, ArrowRight stepped p 23 -> p 24, no app chrome on /read. User stopped further smoke (closed tab, declined reopen).

## Key facts
- Route `/read/<plugin>/<slug>?ch=&page=&mode=&dir=&zoom=` renders a self-contained shell via partials `templates/reader_head|foot|top|bottom.mustache` (own doctype/head/body, Tailwind CDN + Alpine, no Library|Search header, no <main> padding).
- Sticky-toolbar replaced by overlay bars gated on `navShown` (default false). Center third toggles navShown (all modes); left/right thirds keep RTL-aware step(). Dim `p N / M` indicator bottom-center when nav hidden, pointer-events-none.
- Top overlay: back-to-detail control (icon + label, 2026-11-10), chapter <select> (all chapters via /api/chapters, chUrl), page <select> 1..N via goto_page(). Bottom overlay: mode/dir/zoom selects + numbered 1..24 jump buttons or a big `current/total` jump control (focuses top page select) when > 24 pages.
- Zoom: additive `zoom` column on reader_positions (PRAGMA table_info guarded ALTER TABLE for existing DBs; sqlite_orm sync_schema can't ALTER in place). ReaderPosition.zoom, merge_position keeps stored zoom when absent (mirror mode/direction), GET /api/position returns it, POST accepts it. Reader seeds from resume_zoom, posts zoom, chapter-crossing chUrl carries mode/dir/zoom (+ page). Cycle: fit-h (default) | fit-w | 1:1 + zoomPct 50-200% with +/- buttons in bottom bar and keyboard `+`/`-`/`0`.
- Navigation: ArrowLeft/Right = prev/next page in ALL modes (RTL-aware in paged via step(); webtoon via goto_page + scrollIntoView). Home/End = first/last. Escape = close Done overlay. Form controls always guarded first.
- Wheel: fixed overlay bars were eating native wheel events so inner `overflow-auto` shell never scrolled; reader.mustache now scrolls the `[data-shell]` manually when it overflows (fit-w/1:1), fit-h steps pages on wheel (180ms debounce), webtoon is native body scroll (`body overflow-y-auto`, `main min-h-screen`).
- Cross-chapter boundary: step() past last view -> next_ch (page 1) or done=true (Done overlay, back-to-detail link); before first view -> prev_ch with prevChapter.pages (chapters output gained `pages = tonumber(attrs.pages) or 0`) else page 1. MangaDex /api/chapters cached entries lack `pages` until natural cache expiry -> prev lands on page 1 (spec-allowed fallback).
- Taps swallow nothing when done=true; zones/overlays/indicator carry !done guards.

## Verification gaps
Not live-verified: wheel stepping in fit-h, numbered buttons <=24, >24 fallback focus, Done overlay trigger/link, cross-chapter boundary nav, zoom persistence across reopen.

# Related Concepts
- [reader-page: four-mode reader UI + persisted positions](../architecture/reader-page-progress.md): base capability this extends (zoom persistence gap closed here).
- [reader-page: gaps and important facts](../references/reader-page-gaps.md): gap list; zoom-unpersisted item closed by this change.
