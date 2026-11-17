# Tasks: chapter-state

## 1. Store layer

- [x] 1.1 `ChapterEntity` + `ChapterPageEntity` in `store_internal.hpp`, tables in `create_storage` (composite PKs; `chapters.flags INTEGER` default 0)
- [x] 1.2 `ChapterFlag` enum in `store.hpp` (bit0 read, bit1 hidden) next to `StatusFlag`
- [x] 1.3 `chapters_upsert(plugin, slug, vector<ChapterRow>)` — identity upsert, flags preserved, one `OpTrace`
- [x] 1.4 `chapter_pages_replace(plugin, slug, chapter_id, vector<url>)` — delete+insert, one `OpTrace`
- [x] 1.5 `chapter_flags_apply(plugin, slug, ids|all, set, clear) -> map<string,int>` — RMW per row, one `OpTrace`
- [x] 1.6 `chapter_flags_get / _list(plugin, slug)` readers
- [x] 1.7 Boot backfill: unique-match `host/skip/*`→bit1, `host/read/*>=pages`→bit0; log applied/dropped counts
- [x] 1.8 `saved_manga.read_ch` rollup switch: count bit0 rows

## 2. Parse hooks

- [x] 2.1 Hook `chapters_upsert` into the chapter-list parse path (host side, after successful parse; plugin payloads unchanged)
- [x] 2.2 Hook `chapter_pages_replace` into the pages parse path

## 3. API

- [x] 3.1 `POST /chapter/flags` in `http_api_state.cpp` (validate plugin/slug; 400 without ids/all)
- [x] 3.2 `/chapter/state`: `read` from bit0, add `progress` (legacy count), `skip` from bit1
- [x] 3.3 `/chapter/<id>/read`: auto set bit0 when `page >= COUNT(chapter_pages)`
- [x] 3.4 Reader window skip filter + worker mirror: `state_get(kSkip...)` → `chapter_flags_get` bit1

## 4. Detail page UI

- [ ] 4.1 Header actions: mark-all-read / clear-read with confirm
- [ ] 4.2 Row menu: mark-previous-read → resolve ids (number ≤ picked, canonical order) → confirm → one POST
- [ ] 4.3 Check glyph lit from `read` bit; optimistic toggle + re-render from response

## 5. Verify

- [x] 5.1 `tests/test_store.cpp`: upsert-preserves-flags, pages replace, RMW set/clear/all, reader helpers
- [x] 5.2 `tests/test_http.cpp` (or closest suite): `/chapter/flags` 400/ok paths; `/chapter/state` shape
- [x] 5.3 Live: mark 1..N on a real title → checks lit; clear-all → unlit; reader last page auto-lights; skip eye still drives the window
- [x] 5.4 Zero-warning build, full suite green
- [ ] 5.4 Zero-warning build, full suite green
