# Proposal

## Why

Every chapter change in the reader is a full document reload (`window.location` in `gotoChapter()`, boundary `step()`, and the chapter dropdown): the whole page re-renders, Alpine re-boots, and both API calls re-run — a visible flash and lost DOM/scroll state between chapters, even though all chapter data already comes from JSON endpoints. Reading through a long manga means a hard reload every chapter.

## What Changes

- Chapter switches (next/prev at page boundaries, chapter dropdown, keyboard navigation) swap chapter content **in place** via fetch — no document reload.
- Adjacent chapters (next first, then prev) are prefetched in the background after the current chapter renders; switching to a prefetched chapter is instant.
- The URL stays synchronized (`history.pushState` + `popstate`) so refresh, browser back/forward, and shared links still deep-link to chapter+page.
- Reader chrome (top/bottom bars, mode/direction/zoom selection, debounced position persistence) keeps working across in-place switches; lazy-load observers and scroll state are re-armed per chapter.
- No server-side changes: the existing `/api/pages/<plugin>/<slug>/<ch>` and `/api/chapters/<plugin>/<slug>` endpoints already carry everything needed (ordered page URLs, `prev_ch`/`next_ch`, per-chapter page counts).

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `reader-page`: chapter navigation SHALL switch chapters client-side without a document reload, with adjacent-chapter prefetch, and URL, position, and reading-state continuity across the switch.

## Impact

- `templates/reader_script.mustache` — `boot()` refactored into a reusable `loadChapter(n, page)`; `step()`/`gotoChapter()` go in-place; `popstate` handler; small prefetch cache keyed by chapter number.
- `templates/reader_top.mustache` — chapter dropdown uses the in-place switch (markup-level handler change only).
- No server, store, or dependency changes. Golden fixtures regenerate (`CPPISEKAI_UPDATE_GOLDEN=1`) since the reader shell ships inside the rendered pages.
