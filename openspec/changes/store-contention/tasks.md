# Tasks: store-contention

## C1 — coalesced dup-recluster

- [ ] 1.1 `Store::mark_dup_dirty()` + `dup_dirty` flag (`store.hpp`, `store_core.cpp`); `sync_saved_stats` marks instead of recomputing (`api_ctx.hpp:284`)
- [ ] 1.2 scheduler tick: `if (dup_dirty.exchange(false)) recompute_dup_groups()` (`src/update/scheduler.cpp`)
- [ ] 1.3 keep immediate recluster in `save_manga`/`unsave_manga` paths; boot-time first recluster preserved (flag default true)
- [ ] 1.4 unit: mark→tick→exactly one recompute; dup census identical before/after on live DB

## C2 — raw-op mutex split

- [ ] 2.1 `OpTrace`: raw ops take `raw_mutex()` instead of `db_mutex()`; overlap ledger unchanged (`store_ops.hpp`)
- [ ] 2.2 unit: concurrent api_cache + state ops progress while a slow op holds `db_mutex`

## C3 — chapters no-change skip

- [ ] 3.1 seed fingerprint + `chapters_fingerprint` get/set via `plugin_state` (`host/chapters_fp/*`), written inside the upsert txn
- [ ] 3.2 `materialize_chapters`: equal fingerprint ⇒ skip upsert (`api_ctx.hpp`)
- [ ] 3.3 unit: second identical materialize is a no-op (update counters unchanged); changed seed still rewrites; flag-only change still persists (flags ride through existing rows)

## C4 — synchronous=NORMAL (needs explicit user go)

- [ ] 4.1 `PRAGMA synchronous=NORMAL` beside the wal pragma (`store_core.cpp`); note in `data/cppisekai.ini`-adjacent docs if a config comment exists

## Gate

- [ ] 5.1 `xmake -y` clean, full suite 19/19
- [ ] 5.2 daemon restart; browse session; slow-op re-census: `recompute_dup_groups` warns ≈0 outside saves, `state_get` avg < 50 ms; dup census + reader state unchanged
