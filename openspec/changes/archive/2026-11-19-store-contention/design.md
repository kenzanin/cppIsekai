# Design: store-contention

## D1 — coalesced dup-recluster (dirty flag + scheduler tick)

`Store` gains `dup_dirty` (atomic bool, default true so the first boot reclusters) and
`mark_dup_dirty()`; `sync_saved_stats` (`api_ctx.hpp:284`) replaces the inline
`recompute_dup_groups()` with `mark_dup_dirty()`. `save_manga`/`unsave_manga` keep the
immediate recluster (user-visible dedup badge must be correct at click time) and also set
the flag. The existing 60 s scheduler tick in `src/update/scheduler.cpp` gains: if
`dup_dirty.exchange(false)` then `recompute_dup_groups()` — bursts collapse to one run per
tick; a browse session costs 0.

Fallback correctness: flag is in-process only; a crash just delays one recluster, and the
next save/boot recomputes. `dup_group>0` census must be identical before/after on the live DB.

## D2 — separate mutex for raw-handle ops

`OpTrace` ctor takes `raw` (already computed) into the lock decision: raw ops
(`api_cache_*`, `maintenance_vacuum_backup` — own `sqlite3_open` handles, verified
`store_api.cpp:15/52/80/181/225`, `store_maintenance.cpp:37`) skip the global `db_mutex`
and take a second static `raw_mutex()` instead; the `active()` overlap ledger keeps its
existing `mutex()` guard unchanged. ORM ops are untouched. Cross-handle WAL access is
already proven safe by the existing raw paths (`attach_busy` present); the overlap warn
keeps working via the ledger.

Risk: none to the single-connection invariant — raw ops never touched the shared handle;
the mutex only ever over-protected them.

## D3 — no-change skip in the chapters path

`materialize_chapters` computes a cheap fingerprint of the seed set
(`size ^ per-row hash(number,title,url)` via `std::hash` string combine) and compares it to
`store.chapters_fingerprint(plugin, slug)` — a new cached-per-title value stored in
`plugin_state` (`host/chapters_fp/<plugin>/<slug>`, existing KV table, no schema change).
Equal ⇒ skip `chapters_upsert` entirely (flags, pages, updated_at untouched — a re-parse
with identical seeds is a no-op by definition). Different/missing ⇒ current path. The
fingerprint write rides inside `chapters_upsert`'s existing txn.

`chapter_pages` rewrite stays inside the upsert for the changed case (unchanged behavior).

## D4 — synchronous=NORMAL (gated on user go)

One line where the ORM connection is opened (`store_core.cpp` pragma block, next to the
existing `journal_mode=wal`): `PRAGMA synchronous=NORMAL`. Documented tradeoff: safe
against app crash; on OS crash/power loss the last committed txn(s) may roll back to the
WAL checkpoint. `[backup]` hourly snapshot bounds real loss. If the user declines, the
task is dropped and FULL stays — no other task depends on it.

## Test hook

`tests/test_store.cpp`: raw-op concurrency (spawn two threads, one `api_cache_get` loop
one `state_get` loop, assert both progress while a slow `recompute_dup_groups` runs —
regression for D2); fingerprint skip (call materialize-equivalent twice, second is a
no-op — D3); dup-flag coalescing unit (mark → tick → single recompute — D1).
