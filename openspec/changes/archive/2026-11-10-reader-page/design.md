# Design: reader-page

## D1 — Pages API: server-rendered URL list

`GET /api/pages/<plugin>/<slug>/<ch>` returns JSON `{plugin, slug, chapter, mode_hint, pages: ["/cache/img?url=..&plugin=..&slug=..&ch=..&page=N", ...], next_ch, prev_ch}`.

- Alternative considered: client builds `/cache/img` URLs itself from chapter metadata. Rejected — the ratio suffix rule, v3 rel_path, and future host-side transforms must stay host-owned; clients only get opaque URLs.
- Page count comes from the plugin `pages` call (cached via the stale layer like detail/chapters). Unknown chapter → 404 JSON.
- `next_ch`/`prev_ch` from the chapter list so the reader can navigate without refetching the whole list.

## D2 — Reading modes are a client-side layout concern

Four modes: `webtoon` (vertical scroll, one page per row), `paged` with direction `ltr|rtl` (one page per screen), `spread-h` (two pages side by side, `ltr|rtl`), `spread-v` (two pages stacked, paged down).

- All modes consume the same flat URL list; the reader JS picks the layout. No server-side page pairing.
- Spread pairing rule: page 1 renders alone (keeps page 1 on the right in RTL spreads and on top-left in LTR), then pages pair (2,3), (4,5)... An odd tail page renders alone. RTL spread mirrors the pair order.
- Mode + direction persist per manga (see D3); default: `webtoon` if the plugin flags long-strip, else `paged rtl` (common manga default). Plugin flag optional — user override always wins.

## D3 — Position persistence in sqlite

Reader positions live in a `reader_positions` table (`plugin, slug → chapter_id, chapter_number, page, mode, direction, updated_at`), written by `POST /reader/position` — the same request that already nudges the prefetch worker, extended with mode/direction fields (backwards-compatible: absent fields keep stored values).

- Alternative considered: reuse `plugin_state` key-value. Rejected — resume needs structured ordering (badge "continue ch N p M") and mode/direction as separate columns; one small table beats overloading a KV blob.
- Writes are idempotent upserts on the single store connection, via the existing `on_pool` gate. Detail page reads it for the continue-hint badge.

## D4 — Prefetch: reuse the existing worker, no new machinery

Client POSTs position on every page/spread change (debounced ~300ms); the existing worker diffs the sliding window against disk and fetches misses. Webtoon mode additionally nudges with the topmost visible page (IntersectionObserver), so scrolling down keeps ~10 pages ahead warm. Chapter boundary: when the window spills into the next chapter (existing window math already does this), the pages API of the next chapter is fetched lazily on approach, not eagerly.

## D5 — Kept out of scope

- Image transforms per mode (no server-side double-page stitching; CSS pairs them).
- CBZ/zip streaming to the reader (pages stay individual `/cache/img` URLs).
- Reader settings beyond mode/direction (zoom, background color) — later polish.
