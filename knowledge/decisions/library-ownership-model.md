---
type: Decision
title: "Library ownership: RAM for browse, SQLite for saved, plugin memory derived as orphan (IMPLEMENTED)"
description: "IMPLEMENTED 2026-11-23 (branch library-ownership): unsaved pairs route to an in-process map (nothing in SQLite, restart clears); POST /api/library promotes flags/read_max/pages/position/title_meta/host columns in one txn; plugin folder removal GCs only caches+registry so library rows survive as orphans derived from the registry (reinstall revives, /api/library/purge-orphans removes); legacy title table + search/count + per-pair plugin_state keys retired."
tags: [library, store, saved_manga, orphan, ephemeral, plugin-lifecycle, openspec, planned]
generated: { by: agent/mcp, at: "2026-11-23T10:09:03Z" }
status: stable
---

## Status

IMPLEMENTED 2026-11-23 on branch `library-ownership` (openspec change of the same name).

## Model

Two hierarchies, each with its own root:

- **plugin** = registry/namespace root (id, version, enabled, runtime). Reinstalling revives data.
- **saved_manga** = user-data root. Children: `chapters` (flags, read_max), `chapter_pages`,
  `reader_positions`, `title_meta`, `update_attempts` and the per-pair host columns
  (`chapters_fp`, `novel`, `detail_url`).

## Implemented rules

1. A pair with no `saved_manga` row routes to `StoreImpl::ephem` (map, LRU 128 pairs): **nothing** is
   written to SQLite. Chapter identity/flags/read_max, page URLs, position and title_meta live there.
2. `save_manga` promotes that entry in one transaction (`ephem_promote`), carrying pre-save read marks;
   `unsave_manga` erases it.
3. `plugin_prune_absent` now GCs `plugin_state` + `api_cache` + the `plugin` row only — library rows
   survive and are reported as orphans.
4. Orphan is **derived** (`Store::plugin_upstream` -> live/offline/orphan); gates: stale layer, refresh
   (`plugin removed`), scheduler EXISTS query. `/api/library` exposes `state`; grid has an orphan
   badge + filter; `POST /api/library/purge-orphans` wipes them via the existing remove path.
5. Retired: the dead `title` table + `store.search/count` + the sidebar title join (now counts
   `saved_manga` rows), the `host/pos` constant, and every per-pair `plugin_state` family
   (boot-migrated into columns).

## Concurrency fact (graph-verified)

Store calls are **not** thread-confined: `db_pool` has one thread, but `call_cached` reaches
`plugin_upstream`/`api_cache_*` from a plugin-pool thread. Every store method holds `OpTrace`'s
process-wide recursive `db_mutex`, which is what makes the plain map safe.

## Rejected

- Stored orphan bit (registry is the source of truth; bit7 of status_flags remains the escape hatch).
- FK cascade to `saved_manga` (forces a parent row for browse-only titles; table rebuild).
- SQLite TEMP TABLE for browse data (same disappearing act, more SQL for shapes that already have structs).

## Known trade-offs

- Unsaved reading progress is lost on restart (accepted; upgrade path = persist only `reader_positions`).
- Image cache namespaces key on the plugin display name, so a rename loses cached images (data intact).
