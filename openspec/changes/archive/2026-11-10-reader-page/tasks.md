# Tasks

## 1. Pages API

- [x] 1.1 `GET /api/pages/<plugin>/<slug>/<ch>` in `src/http/http_api.cpp`: chapter page list via stale-layer plugin `pages` call → JSON {pages: [/cache/img urls], next_ch, prev_ch}; unknown chapter = 404 JSON; unit test with fake plugin pages (order, next/prev, 404)

## 2. Position persistence

- [x] 2.1 `reader_positions` table (plugin, slug → chapter_id, chapter_number, page, mode, direction, updated_at) in `src/store/`; upsert + read helpers behind the store gate; unit test round-trip
- [x] 2.2 `POST /reader/position` accepts optional mode/direction (absent = keep stored), upserts, still nudges worker; detail template continue-hint badge from stored position; unit test for partial-update semantics

## 3. Reader UI

- [x] 3.1 `templates/reader.mustache` + reader JS: render pages from `/api/pages`, four modes (webtoon scroll w/ lazy-load, paged ltr/rtl, spread-h ltr/rtl with first-page-alone pairing, spread-v stacked), keyboard/tap nav, mode switcher persisted via position POST
- [x] 3.2 Wire `/view/manga/<plugin>/<slug>?ch=<n>` to the reader template; resume at stored position (ch+page) and restore mode/direction; `?ch` overrides resume chapter
- [x] 3.3 Debounced (~300ms) position POST on page change + IntersectionObserver nudge in webtoon mode; verify prefetch keeps ~10 pages ahead (manual curl: pages land in `img/<plugin>/manga/<slug>/ch<n>-<id8>/` while reading)

## 4. Verify

- [x] 4.1 Full ctest green, zero new warnings; live smoke: open reader, switch all four modes, resume after daemon restart; `openspec validate --specs`
