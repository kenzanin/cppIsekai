## Context

`saved_manga{ plugin, slug, name, cover, added_at }`. Read state per chapter: `plugin_state` key `host/read/<chapter-uuid>` storing max page. `host/title_meta` holds enrichment. Reader position route exists (`/reader/position`, `reader-page`). Refresh route `/api/refresh/<plugin>/<slug>` re-fetches detail+chapters via `call_cached`.

Chapter UUIDs are plugin-scoped, not manga-scoped — there is no stored chapter→manga map.

## Goals / Non-Goals

**Goals:** stat columns on `saved_manga`, sync from plugin source, `has_update` lifecycle, reader/continue-read clears it.

**Non-Goals:** FTS5, dedup, read-parallel DB (deferred; tracked in OKF as `decisions/db-read-parallel`). Per-chapter table. Plugin ABI changes.

## Decisions

**D1 — Denormalize on `saved_manga`, not join tables.** Status/total/read are single-valued per saved manga; columns keep grid query trivial. Filtered data stays columns per `decisions/normalization-contract`.

**D2 — Migration additively.** New columns via `pragma.table_info` + `ALTER TABLE … ADD COLUMN` in `Store::Store` boot, mirroring existing `api_cache.payload` / `reader_positions.zoom` ponytail blocks. `sync_schema` 8-column mismatch is avoided by not adding them to ORM entity until migrated? -> no: add to ORM entity AND do additive ALTER, as api_cache/zoom pattern does.

**D3 — `read_ch` derivation.** Two paths, belt and braces:
1. On detail open / refresh: walk the chapters envelope (already fetched) and count chapters whose `host/read/<id>` >= their page count (or > 0? user decided read counts when completed — check lit at >= total; but partial progress shown as max page). `read_ch` = count of completed chapters.
2. Reader position POST can bump nothing per-manga without a chapter→manga map; rely on (1) instead — recompute on open/refresh. Cheap enough: chapters list is already in the envelope.

`total_ch` = chapters envelope length (or `detail.total` if plugin reports it). `status` = envelope `detail.status` if present, else `chapters`-derived? -> use `detail.status` only (user said source = plugin manga source, detail envelope carries `status` on our plugins).

**D4 — `has_update`.** Set in refresh when new `total_ch` > stored `total_ch` (or chapters envelope contains a max chapter number > stored max). Store also `max_chapter_seen`? -> keep minimal: `has_update` + `updated_at` only; `updated_at = time(refresh) when envelope changed (name/cover/status/total differ)`. Clear `has_update` on: reader position POST, Continue-read click, refresh with no growth. This matches prior OKF `library-page` decision (clear_update on open/read).

**D5 — `mark_read(id)`.** `read_ch = total_ch`, `has_update = 0`; invoked from the detail page "Done" action if/when added, else unused this change (keep store method only if a caller lands; otherwise skip — YAGNI).

**D6 — Write path single-writer.** All writes go through `on_pool(db, …)` (db_pool=1). No read-parallel work here.

**D7 — Ordering.** Merge BEFORE or AFTER `library-grid-controls`? Controls degrade gracefully without stats, so order is flexible; this change must land on its own first if controls want real badges in the same release.

## Risks / Trade-offs

- envelope `status` shape varies across plugins — normalize via existing `str_field` helpers, unknown values stored as-is (grid chip colors map known set, unknown = gray).
- `read_ch` recompute requires the chapters envelope; if the plugin is down and refresh fails, stats stay stale (correct — stale-serve philosophy).
- `ALTER TABLE` on every boot guarded by `table_info` probes (existing pattern).
