---
type: Decision
title: "Store: reuse one raw sqlite handle for api_cache (measured 0.4ms/call saved)"
description: "api_cache raw handle reuse + enrich hot-path round-trip collapse + guarded stat writes; cache-hit 1.62->1.0ms, detail row-serve 2.5-3.9->2.1ms, WAL churn zero."
tags: [store, sqlite, performance, api-cache]
generated: { by: agent/mcp, at: "2026-11-26T06:30:31Z" }
status: stable
---

## Measured (2026-11-26, data/library.db 13 MB / 124 cache rows)
- `sqlite3_open` + `close` per raw op: **0.394 ms**; same query on a reused handle: **0.006 ms** (~65x).
- `api_cache_get` fires on **every** plugin call (`call_cached`), plus `api_cache_put` on each miss, `api_cache_clear`, `cache_prune_expired`.

## Change 1: raw-handle reuse
`StoreImpl` holds one lazily-opened `sqlite3* raw` (`raw_db()`), busy-handler attached once, closed in `~StoreImpl`. Used by the three `api_cache_*` ops and `cache_prune_expired` — exactly the ops OpTrace already routes to `raw_db_mutex`, so the single-owner invariant holds. `maintenance_vacuum_backup` keeps its own handle (VACUUM, rare).

## Change 2: enrich hot path (same commit family)
- `saved_detail_response`: 4 pooled reads -> **1** (tuple snapshot: saved + novel flag + detail_url + title_meta).
- `apply_chosen`: 2 -> **1** (title_meta + saved together); and when nothing renamed, the original envelope bytes are returned instead of a re-serialization round-trip.
- `saved_stats_update` / `saved_read_rollup`: **guarded writes** — both run on every row-served detail open; an unchanged row no longer emits a WAL frame per open.

## Result
- detail (cached row, 20-run avg): **2.5-3.9 ms -> ~2.1 ms**; search cache-hit 1.62 -> ~1.0 ms (raw-handle change).
- WAL after 30 detail opens: delta 0 bytes (writes now meaningful-only).
- 22/22 tests; no new warnings.

## Not done (ceilings)
- `orphan_list` / `update_candidates` / `plugin_prune_absent` / saved_detail_meta raw ops keep their own handles (db_mutex family; sharing = OpTrace list change for per-tick ops).
- No secondary indexes at 31 rows; read-parallel deferred (`decisions/db-read-parallel`).
- harimanga empty-envelope cache poisoning fixed in the same pass (positive page-ready gate + error-not-200 on empty parse).

generated: { by: "kit-agent", at: "2026-11-26" }
