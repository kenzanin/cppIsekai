---
type: decision
title: "Reader keyboard map: arrows pan, page only at the edge"
description: "Reader keys reworked 2026-11-20: ArrowDown/Up pan the visible image (80% of the pane) and turn the page only when already at the scroll edge; PageUp/PageDown and Space page explicitly; webtoon arrows scroll the strip in small steps. One _scroller() picks the shell pane or the document, because the old helper scrolled window while the pane is an h-screen overflow-auto box."
tags: [reader, keyboard, navigation, scroll, spread, webtoon, static-js]
generated: { by: agent/mcp, at: "2026-11-20T13:01:05Z" }
status: stable
---

## Decision

Keyboard navigation in the reader (static/reader.js, keys()) is pan-first:

| Context | Key | Action |
|---|---|---|
| paged / spread-h / spread-v | ArrowDown, ArrowUp | pan the current image by 80% of the visible pane; at the bottom/top edge it turns the page (step +1/-1) |
| paged / spread | PageDown, PageUp, Space, Shift+Space | next / previous page (Space is the standard reader key; Shift reverses) |
| paged / spread | ArrowRight, ArrowLeft | next / previous page, flipped by dir for RTL |
| paged / spread | Home, End | first / last page |
| webtoon | ArrowDown, ArrowUp | scroll the strip by 15% of the viewport (default browser scrolling is not reliable: keydown reaches Alpine, the default action did not scroll) |
| webtoon | Space, Shift+Space, PageDown, PageUp | scroll one screen |
| webtoon | ArrowRight, ArrowLeft | next / previous page (goto_page + scrollIntoView) |
| all | +, =, -, 0 | zoom in / out / reset to 100% custom |

Form controls win: SELECT/INPUT/TEXTAREA targets return before any binding, so Space in the chapter dropdown still opens it.

## Why

The user's report: "arrow key sekarang panah bawah jadinya next page harusnya scroll gambar ke bawah" — with fit-w or a zoomed preset the image is taller than the pane, so the vertical arrows have to pan it; turning the page belongs at the edge only. Two latent bugs surfaced in the same pass:

1. `_scrollBy` scrolled `window`, but in paged/spread the pane is `h-screen overflow-auto` (shellClass), so PageUp/PageDown did nothing. Fixed with `_scroller()`: webtoon → `document.scrollingElement`; otherwise the shell when `scrollHeight > clientHeight + 2`, else the document.
2. `_atEdge(d)` returns true when the pane has no overflow (fit-h), so vertical arrows degrade to plain paging instead of a dead key.

## Traps

- Smooth scrolling (`behavior:'smooth'`) does not animate in an unfocused/background tab — scrollTop stays 0 and a working handler reads as broken. Panning scrolls instantly, which is also the better feel for key repeat.
- `static/reader.js` is brotli-served: every edit needs `brotli -q 11 -f -o static/reader.js.br static/reader.js` or the browser keeps the old body.
- Verify live, not by reading the diff: `[data-shell].scrollTop` / `scrollHeight - clientHeight` and the rendered label ("p 3 / 43", "p 4–5 / 43") are the evidence.

## Verified 2026-11-20 (real Chrome, chapter 1 of a 43-page title)

paged fit-w: ↓ 0→494→988, ↓ at edge → page +1 with scroll reset, ↑ at top → page -1. fit-h (no overflow): ↓ pages directly. spread-h 1:1 with pairFrom anchor: "p 4–5" ↓ pans, → "p 6–7" resets scroll, ↑ returns to "p 4–5". spread-v 1:1: pans to 1482 of 1877, ↓ at bottom → next pair. webtoon: ↓ 2078→2170→2263 (92px steps), PageDown 2725, → p3. Suite 19/19. Commit b3f4a6f.

# Related Concepts
- Related to [reader-fullscreen: full-screen reader chrome + zoom persistence (implemented, uncommitted)](../architecture/reader-fullscreen.md)
