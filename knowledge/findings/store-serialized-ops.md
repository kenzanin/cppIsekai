---
type: Finding
title: "database is locked: concurrent store ops on the one shared connection"
description: "One shared sqlite connection: OpTrace serializes calls, but explicit transactions also need TxGuard or one throw wedges every later BEGIN."
tags: [sqlite, lock, store, concurrency, database-is-locked, fixed, trace, transaction, txguard, busy-snapshot]
generated: { by: agent/mcp, at: "2026-11-17T21:50:39Z" }
status: stable
---

## Symptom
Detail pages and /api/library returned 500 with `on_pool failed: database is locked` under parallel browsing, while the log's busy-handler retry lines (store_busy) stayed silent. Plugin search worked; only store-touching routes died.

## Root cause
The store owns ONE sqlite handle and sqlite_orm runs each operation as several statements. Two threads interleaving statements on that handle -- observed pair: `plugin_enabled` from two Crow workers (t7304/t9519), and `state_set` (db worker t125) overlapping `api_cache_put`/`api_cache_get` (plugin threads) -- make sqlite fail with SQLITE_BUSY_SNAPSHOT. That code is returned immediately and is NOT passed to the busy handler, which is why a 3s (later 15s) busy_timeout never helped and never logged.

Diagnosis came from store_detail::OpTrace: per-op names + wall time + overlap detection across threads. 76 overlap events across 80 mixed requests; 3 of them ORM-vs-ORM on the same connection.

## Fix
`src/store/store_ops.hpp` (new): OpTrace is both the tracer and the serializer -- it takes one process-wide `std::recursive_mutex` for the duration of the call, then logs `store: <op> <ms> (t<id>)` at **debug** level once the op finishes.
Every public Store method now opens with `const store_detail::OpTrace _op{"<name>"};` (~37 methods across store_api/store_search/store_enrichment).

## Why debug, not info
The user asked for permanent instrumentation without log cleanup: the `[log] level` knob in the config registry already exists (src/config/config.hpp, applied in src/main.cpp via spdlog::set_level). Per-op lines sit at debug, so they are silent by default and read by setting `[log] level = debug` in the ini or `CPPISEKAI_LOG_LEVEL=debug`. Only >50ms ops and (would-be) same-connection overlaps warn.

## Verification
- Before: 1 lock error + 76 overlap events under 24x6 mixed load.
- Named culprit: `plugin_enabled` called from 3 distinct Crow worker threads, plus `upsert_plugin` (79ms) off the db worker.
- After: 4-5 way parallel load (view page, detail, read POST, plugin search, chapters) x 8-10 rounds -> 0 lock errors, 0 same-connection overlaps, 0 on_pool failures. Slow warnings limited to bounded work: recompute_dup_groups ~130ms, upsert_plugin ~113ms, api_cache_put ~53ms.
- `xmake test` 18/18 pass; zero-warning build (also completed saved_row's authors/artists/tags/synopsis_display designators in declaration order).

## Note
Serializing means a 130ms op (recompute_dup_groups, envelope sync) now blocks readers for that window. Acceptable at ms scale; if it ever bites, move the dup recompute onto the db worker and out of the request path.

# Related Concepts
- Related to [Store migrated to sqlite_orm (Pimpl)](../architecture/store-orm-migration.md)


## 2026-11-18: second failure mode -- a dangling transaction wedges the whole DB

Same one-connection store, different failure. Symptom: every title's `/api/chapters` and `/api/pages` 500 with `cannot start a transaction within a transaction` until the daemon restarts; first bad line in the log was `store: chapters_upsert 1.0 ms [error] on_pool failed: database is locked`.

Chain: `chapters_upsert` opened `begin_transaction()`, then `st.replace` hit `SQLITE_BUSY_SNAPSHOT` (deferred txn read its snapshot, then wrote), the exception escaped the scope without `commit` or `rollback`, and from then on every `BEGIN` on that connection failed -- permanently, for all titles. OpTrace serializes calls but does not close transactions, so the mutex gave no protection here.

Fix (6edebdd) in `src/store/store_ops.hpp` + `store_search.cpp`:
- `store_detail::TxGuard` RAII: rolls back on scope exit unless `commit()` ran. Applied to `chapters_upsert`, `chapter_pages_replace`, `chapter_flags_apply`.
- `chapters_upsert` now reads all existing rows in ONE query BEFORE `begin_transaction`, so the deferred txn never upgrades a snapshot (that upgrade was what produced BUSY_SNAPSHOT).

Regression test (68f01e0): seed a row, install a `BEFORE INSERT ON chapters` trigger that RAISEs, `CHECK_THROWS(store.chapters_upsert(...))`, drop the trigger, then assert the connection still works (`chapter_flags_apply` returns 1, a later upsert lands). Removing the guard turns the test red; restoring it back to green -- verified both ways.

Rule for any new explicit transaction: wrap in TxGuard and read before BEGIN.
- [Chapter state: one flags bitfield per chapter row, chapters as metadata hub](../architecture/chapter-state-flags.md): The chapter-state store methods are what exposed the dangling-transaction wedge; TxGuard now wraps them.
