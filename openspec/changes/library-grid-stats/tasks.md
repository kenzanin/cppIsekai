## 1. Store schema

- [x] 1.1 `src/store/store_internal.hpp`: extend `SavedMangaEntity` + `SavedManga` struct with `status std::string TEXT default 'ongoing'`, `total_ch INTEGER`, `read_ch INTEGER`, `has_update INTEGER`, `last_read_at INTEGER`, `updated_at INTEGER`.
- [x] 1.2 Boot migration: `pragma.table_info("saved_manga")` per column, `ALTER TABLE` ADD COLUMN with defaults (ponytail comment); guard on existing columns.
- [x] 1.3 `saved_list()` returns the stat fields; test in `tests/test_store.cpp` (or nearest) asserting defaults + roundtrip.
- [x] 1.4 Store helper `saved_stats_update(plugin, slug, {status, total_ch, updated_at, has_update})` + `saved_read_rollup(plugin, slug, read_ch, last_read_at)` — writes only provided fields.

## 2. Sync hooks

- [x] 2.1 `/api/refresh` (`http_api_title.cpp` / `http_api_admin.cpp` — wherever `/api/refresh/<plugin>/<slug>` lives): after successful refresh, read the fresh detail envelope (already available) and call `saved_stats_update`; set `has_update=1` iff new total > old.
- [x] 2.2 Detail open path (`/api/detail` / SSR context build): same derivation on stale-serve (cheap; reuse stored envelope if unchanged).
- [x] 2.3 `read_ch` recompute: walk chapters envelope, count chapters with `host/read/<id>` >= chapter page count (fall back to >0 when page count unknown); call `saved_read_rollup`.
- [x] 2.4 Reader position POST (`/reader/position`): `has_update=0` + `last_read_at=now` for that (plugin, slug); same on the detail-page "Continue read" click handler path if it differs.
- [x] 2.5 Reader open also triggers stats refresh if `updated_at` stale > N days? -> NO (YAGNI; refresh button covers it).

## 3. Grid feed

- [x] 3.1 `http_pages.cpp` saved rows: emit `data-*` (name, plugin, status, total, read, update, added, lastread) — mirrors `library-grid-controls` 1.1; whichever change lands second reconciles.
- [x] 3.2 `GET /api/library` JSON includes stat fields.

## 4. Verification

- [ ] 4.1 `ctest --test-dir build -R store --output-on-failure`; manual smoke: save → refresh with plugin that reports growth → badge appears; open reader → badge clears.
