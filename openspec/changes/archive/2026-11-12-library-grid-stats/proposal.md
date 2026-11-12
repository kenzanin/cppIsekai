## Why

The Library grid needs per-manga stats that survive plugin refreshes: title-level `status` (ongoing|hiatus|complete|canceled), `total_ch`, `read_ch`, `has_update`, `last_read_at`, `updated_at`. Today these live nowhere locally — `/api/refresh` re-fetches the detail envelope but discards the numbers, and read progress is stored per chapter UUID under `host/read/<id>` with no manga rollup. Without this data the grid badges/filters planned in `library-grid-controls` have nothing to render.

## What Changes

- `saved_manga` gains columns: `status TEXT DEFAULT 'ongoing'`, `total_ch INT DEFAULT 0`, `read_ch INT DEFAULT 0`, `has_update INT DEFAULT 0`, `last_read_at INT DEFAULT 0`, `updated_at INT DEFAULT 0` (unix seconds).
- Sync hooks: after a successful refresh (`/api/refresh/<plugin>/<slug>`) and after detail open, write envelope-derived `status`, `total_ch`, `updated_at` from the plugin's `detail` response; `read_ch` computed as count of `host/read/<chapter-id>` rows whose chapter belongs to the manga — in practice: derived on open from the chapters envelope + stored read state (see design D3), or bumped incrementally by reader position posts.
- `has_update=1` set when refresh reports a higher `total_ch` than stored (or a chapter number higher than `max_seen`); cleared by `/reader/position` POST and by "Continue read" click (mark-as-seen), consistent with prior OKF `library-page` decision (`clear_update(id)`).
- `mark_read(id)` semantics: `read_ch = total_ch` + `has_update = 0`.
- `library-grid-controls` renders these fields when present.

No new dependencies. No plugin ABI change.

## Capabilities

### New Capabilities
None (extends library-save + store).

### Modified Capabilities
- `library-save`: saved entries carry stat fields; refresh syncs them; mark-read clears `has_update`.
- `reader-page`: reader position POST clears the entry's `has_update`.

## Impact

- `src/store/store_internal.hpp` (+ sync_schema additive columns via `pragma.table_info` + `ALTER TABLE`, same ponytail pattern as `api_cache.payload` / `reader_positions.zoom`), `store_*.cpp`, `http_pages.cpp` (pass fields to grid), `/api/refresh` handler, reader position handler, detail page boot, detail open path.
- `saved_list()` signature changes to return stat fields → grid JSON keys `data-*` feed `library-grid-controls`.
- DB concurrency: all writes stay on `db_pool` (single writer). Reads of these columns ride the same pool for this change (read-parallel is explicitly out of scope — see library-dedup note).
