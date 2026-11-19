# Proposal: store-contention

DB-path audit 2026-11-19 (successor to `hot-path-perf`, archived same day). No requirement
changes, no user-visible behaviour change except one durability toggle gated on explicit
approval. Tasks only — no spec deltas.

## Why

Evidence is the daemon's own slow-op log (1335 warns, threshold 50 ms, `store_ops.hpp`
kSlowOpMs) plus source reading. The single-connection serialization (#5463) is correct and
stays; the problem is three heavy passengers making trivial ops queue behind them:

| op | n (warns) | avg | max | note |
|---|---|---|---|---|
| `state_get` | 360 | 227.7 ms | 719 ms | 1-row PK lookup; the time is mutex queue wait |
| `state_set` | 127 | 102 ms | 1293 ms | same |
| `recompute_dup_groups` | 201 | ~200 ms | 231 ms | runs on EVERY detail open (dedup 2.3) |
| `api_cache_get`/`put` | 217 | ~50–100 ms | — | fetch/search path queues behind library writes |
| `chapters_upsert` | 44 | — | 143 ms | re-runs on unchanged envelopes |

- **C1 — `recompute_dup_groups` ≈200 ms per detail open.** It tokenizes the whole
  library, runs one FTS `MATCH` query per row (71 rows ⇒ 71 MATCHes), union-find, and a
  write txn (`src/store/store_dedup.cpp:200-290`). `sync_saved_stats` triggers it on every
  detail envelope (`src/http/api_ctx.hpp:284-287`) — 201 redundant full reclusters while
  browsing, each holding the global mutex.
- **C2 — raw-handle ops serialize on the ORM mutex for nothing.** `api_cache_*` and the
  backup `VACUUM` open their own sqlite handles (`store_api.cpp:15,52,80`,
  `store_maintenance.cpp:37`) but `OpTrace` takes the global `db_mutex` regardless
  (`store_ops.hpp:74`) — the `raw` flag only downgrades a warning. Cache reads queue
  behind library writes though they never touch the shared connection.
- **C3 — `chapters_upsert` re-runs on unchanged input.** `apply_read_rollup` →
  `materialize_chapters` (`api_ctx.hpp:218`) feeds every seen chapters envelope into the
  upsert: diff vs 8.7k-row table + `chapter_pages` delete/reinsert, producing identical
  state (~143 ms bursts + write amplification).
- **C4 (gated) — `synchronous=FULL` in WAL.** Committing `state_set`/`position_set`
  fsyncs every write. `NORMAL` is the standard WAL pairing; with app-level crashes safe,
  power-loss may lose the last txn(s). The 2026-11-18 hourly backup (`[backup]`) bounds
  the loss. Requires explicit user go — durability is a product call.

## Scope

- `src/store/store_ops.hpp` (mutex split), `src/store/store_dedup.cpp` +
  `src/http/api_ctx.hpp` (dirty-flag recluster), `src/store/store_chapters.cpp` +
  `src/http/api_ctx.hpp` (no-change skip), `src/store/store_core.cpp` (pragma).
- No schema changes, no new tables, no index changes (PKs already cover every hot query —
  verified via `sqlite_master` + autoindexes).

## Non-goals

- No busy_timeout tuning, no connection pool, no read replica: the mutex design stays
  (memory #5463/#5465; BUSY_SNAPSHOT history #5614).
- No `chapter_flags_list` → SQL-count micro-opt (one already-serialized op; map is small).
- No library `count`/`search` skip in saved_mode (2 cheap queries, 1 warn ever).

## Verification

- Full suite 19/19; daemon restart; slow-op warn delta re-censused from
  `data/cppisekai.log` after a browse session (C1/C2 targets: `recompute_dup_groups`
  warns ≈ 0 outside saves, `state_get` avg < 50 ms).
- Duplicate-group results identical before/after (C1): `select count(*) ... dup_group>0`
  unchanged on the live DB.
- Reader/chapter state unchanged after forced same-envelope re-open (C3).
