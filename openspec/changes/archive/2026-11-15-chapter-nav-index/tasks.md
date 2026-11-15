# Tasks: chapter-nav-index

## 1. Pages route: accept the chapter id

- [x] 1.1 Parse an optional `ch_id` query parameter in the `/api/pages/<plugin>/<slug>/<ch>` handler
      (`src/http/http_api_content.cpp`); keep the number-only path when it is empty
- [x] 1.2 When `ch_id` is present and matches a list entry, use that entry's id for the plugin `pages`
      call and for the response's image URLs (design D2); otherwise keep the first-number-match
- [x] 1.3 Pass the resolved id into `PagesCtx` so `build_pages_response` does not re-resolve by number
- [x] 1.4 Make the `pages` cache key distinguish duplicates: the `pages` params object carries the
      resolved id (design D6); the `chapters` call stays keyed by title id only

## 2. Neighbours by index

- [x] 2.1 In `build_pages_response`, build one stable number-ordered copy of the list and locate the
      target entry in it (design D3)
- [x] 2.2 `prev`/`next` are the neighbouring entries in that order — same number is allowed, so a
      duplicate group yields consecutive stops
- [x] 2.3 Emit `prev_ch_id` / `next_ch_id` next to `prev_ch` / `next_ch` (design D4); drop the id
      fields when the neighbour does not exist
- [x] 2.4 Keep the 404 path: unknown number and unknown `ch_id` both answer
      `{"ok":false,"error":"unknown chapter"}`

## 3. Reader: navigate by id

- [x] 3.1 `loadChapter` sends `ch_id` (from the detail link, a neighbour id, or the dropdown entry)
      and takes `prev_ch_id` / `next_ch_id` from the response (design D5)
- [x] 3.2 `chUrl` writes `ch` **and** `ch_id` so refresh and back/forward keep the exact release
- [x] 3.3 `_prefetch` is keyed by chapter id; `_prefetchAdjacent` seeds both neighbour ids
- [x] 3.4 The neighbour guards test the id, not the number (a sibling has the same number)
- [x] 3.5 The chapter dropdown passes the entry's id along with its number

## 4. Tests

- [x] 4.1 `tests/test_pages.cpp`: fixture with two entries numbered 24 between 23 and 25 — assert
      index-based `prev`/`next` for both entries and the sibling ids in `prev_ch_id`/`next_ch_id`
- [x] 4.2 Assert the number-only path still resolves to the first entry (legacy behaviour, D1)
- [x] 4.3 Keep the existing descending-list test green (f8bcbc9 behaviour must not regress)

## 5. Verification

- [x] 5.1 `xmake -y` clean under `-Wall -Wextra`; `xmake test` full suite green
- [x] 5.2 Live: mangadex title with duplicated numbers — step from the first entry of a duplicate
      pair into the second, then back; confirm pages differ and the URL keeps both `ch` and `ch_id`
- [x] 5.3 Live: mangafreak 24a/24b the same way
- [x] 5.4 Live: a number-only `/read/...?ch=24` URL still opens the first entry (fallback intact)
- [x] 5.5 Restart the daemon on the new binary and confirm `/health` plus one reader page load
