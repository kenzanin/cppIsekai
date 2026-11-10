# Proposal: reader-page

## Why

`/view/manga/<plugin>/<slug>` routes to nothing: there is no reader template, so cached pages cannot be read in the browser. All the backend pieces exist (sliding-window math `src/reader/window.hpp`, prefetch worker `src/reader/worker.hpp`, v3 cache layout with pages at `img/<plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext>`, `/reader/position` + `/chapter/<id>/skip` endpoints), but nothing wires them to a page the user actually reads. Position state is also in-memory only: "continue reading" is lost on daemon restart.

## What Changes

- Reader page at `/view/manga/<plugin>/<slug>?ch=<n>` with **four reading modes**:
  1. **Webtoon** — vertical scroll, one page per row, lazy-loaded (long-strip manhwa/webtoons).
  2. **Paged LTR/RTL** — one page at a time, tap/arrow navigation; RTL variant flips direction (right-to-left for Japanese manga).
  3. **2-page spread horizontal** — two pages side by side, paged left-to-right or right-to-left (RTL); single first page shown alone so page 1 stays on the right in RTL.
  4. **2-page vertical** — two pages stacked top-to-bottom per screen, paged downward; odd-tail page handled.
- `GET /api/pages/<plugin>/<slug>/<ch>` — ordered image URL list for one chapter (server-rendered `/cache/img?...` URLs so ratio suffixes and v3 paths stay host-controlled).
- Mode + last position (chapter, page) persisted per manga; reader resumes on open; detail page shows a "continue ch N p M" hint.
- Prefetch wiring: client POSTs `/reader/position` on page change → existing worker prefetches the sliding window → scrolling without spinners.

## Capabilities

### New: reader-page
Reader UI: four reading modes (webtoon, paged LTR/RTL, 2-page spread horizontal RTL-aware, 2-page vertical), chapter navigation, position persistence, prefetch nudging.

## Impact

- `src/http/` — new pages API endpoint + reader template render; position endpoint gains persistence hook.
- `src/reader/` — position store (sqlite via existing store gate), no window-math changes.
- `templates/` — `reader.mustache` + small JS (mode switching, lazy-load, keyboard/tap nav).
- `src/store/` — reader positions table (or `plugin_state` reuse).
- No plugin ABI, config, or cache layout changes.
