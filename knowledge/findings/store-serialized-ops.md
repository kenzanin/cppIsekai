---
type: Finding
title: "database is locked: concurrent store ops on the one shared connection"
description: "Root cause of the intermittent 'database is locked' browse failures (detail pages, /api/library) under parallel traffic: store methods ran on the single sqlite_orm handle from several threads at once, producing SQLITE_BUSY_SNAPSHOT which bypasses the busy handler. Fixed 2026-11-16 by serializing every store op behind one recursive mutex in store_detail::OpTrace (65f8ff5)."
tags: [sqlite, lock, store, concurrency, database-is-locked, fixed, trace]
generated: { by: agent/mcp, at: "2026-11-16T11:42:18Z" }
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
