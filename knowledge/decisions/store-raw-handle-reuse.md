---
type: Decision
title: "Store: reuse one raw sqlite handle for api_cache (measured 0.4ms/call saved)"
description: "api_cache raw sqlite handle opened once per process instead of per call: 1.62ms -> 1.0ms cache-hit path, WAL 13.4MB -> 1.7MB."
tags: [store, sqlite, performance, api-cache]
generated: { by: agent/mcp, at: "2026-11-26T06:02:29Z" }
status: stable
---

## Measured (2026-11-26, data/library.db 13 MB / 124 cache rows)
- `sqlite3_open` + `close` per raw op: **0.394 ms**; same query on a reused handle: **0.006 ms** (~65x).
- `api_cache_get` fires on **every** plugin call (`call_cached`), plus `api_cache_put` on each miss, `api_cache_clear`, `cache_prune_expired`.

## Change
`StoreImpl` holds one lazily-opened `sqlite3* raw` (`raw_db()`), busy-handler attached once, closed in `~StoreImpl`. Used by the three `api_cache_*` ops and `cache_prune_expired` — exactly the ops OpTrace already routes to `raw_db_mutex`, so the handle keeps the same single-owner invariant (no new locking). `maintenance_vacuum_backup` keeps its own short-lived handle (VACUUM is a long exclusive-ish txn; rare path).

## Result
- Cache-hit plugin call: **1.62 ms -> ~1.0 ms** (avg of 20, two runs), i.e. ~35-40% off the hottest path.
- `library.db-wal` dropped 13.4 MB -> 1.7 MB: the open/close churn was keeping the WAL from settling.
- 22/22 tests; `overlapping access` warnings: 0.

## Not done (ceilings)
- `orphan_list`, `update_candidates`, `plugin_prune_absent`, `saved_detail_meta` still open their own handle: they take `db_mutex` (not `raw_db_mutex`), so sharing the raw handle would need an OpTrace list change for a per-tick/per-save op. Low value.
- No secondary indexes: library is 31 rows; `plugin(plugin_id)` is scanned by EXISTS but the table is ~45 rows. Add when a real library hits thousands.
- Read-parallel (multi-connection) stays deferred per `decisions/db-read-parallel`.

## Related bug found while verifying
harimanga served a **cached empty envelope** (ok=true, name="") for a whole TTL: right after boot `goto` could land on `about:blank`, and the old guard "title != Just a moment" passes there. Fixed in the plugin: wait for `readyState==='complete' && host match && not challenge`, and detail now errors on an empty parse instead of returning 200.

generated: { by: "kit-agent", at: "2026-11-26" }

# Related Concepts
- [database is locked: concurrent store ops on the one shared connection](../findings/store-serialized-ops.md): refines the single-connection store: raw handle reuse for api_cache ops
- [DB read-parallel deferred: keep single-writer db_pool; read-only scans on demand](db-read-parallel.md): keeps write path single-writer; read-parallel still deferred by this decision
