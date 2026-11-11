---
type: Decision
title: "library-grid-stats: saved_manga stat columns + envelope-derived sync"
description: "saved_manga carries plugin-derived stats (status/total_ch/read_ch/has_update/last_read_at/updated_at); refresh+detail-open sync reuses stale-served envelopes; reader touch clears has_update; read_ch recomputed on open/refresh only, never per page-turn."
tags: [store, library, sqlite, sync, has-update, openspec, grid]
generated: { by: agent/mcp, at: "2026-11-11T16:59:41Z" }
status: stable
---

Shipped 2026-11-11 (openspec change `library-grid-stats`, 11/11 tasks, 16/16 ctest). Store-layer implementation of the [[architecture/library-page]] stat design.

## Schema (src/store/store_internal.hpp, store_core.cpp)
- `saved_manga` += `status TEXT DEFAULT 'ongoing'`, `total_ch/read_ch/has_update/last_read_at/updated_at INTEGER DEFAULT 0` (unix seconds).
- Boot migration: `pragma.table_info("saved_manga")` guard + `ALTER TABLE ADD COLUMN` in `Store::Store` -- sqlite_orm `sync_schema` will not ALTER existing tables (same ponytail pattern as `api_cache.payload` / `reader_positions.zoom`).

## Store helpers (src/store/store_search.cpp)
- `saved_stats_update(plugin,slug,status,total_ch)`: `has_update=1` iff `total_ch` grows; `updated_at` bumps only on a real change; empty status keeps the stored one.
- `saved_read_rollup(plugin,slug,read_ch,ts)`: `ts==0` leaves `last_read_at` untouched.
- `saved_clear_update(plugin,slug)`: reader touch -- stamps `last_read_at=now` ALWAYS (flag or not; do not early-return on `has_update==0`), clears `has_update` and bumps `updated_at` only when it was set.
- `saved_list`/`saved_get`/`/api/library`/home-grid ctx carry all six fields.

## Sync points (src/http/api_ctx.hpp `sync_saved_stats`, global scope)
- `/api/refresh/<plugin>/<slug>` success and `/api/detail` HTTP 200 both call it with the detail+chapters envelopes already in play (stale-served api_cache; zero extra network). Absent chapters cache => stats untouched (stale-serve philosophy).
- `read_ch` = count of envelope chapters with `host/read/<id> >= pages` (fallback `>0` when pages unknown) -- detail-open/refresh ONLY. Reader position POST never recomputes (no chapter->manga map); next open/refresh heals.
- Reader-open stats refresh when `updated_at` stale: rejected, YAGNI -- refresh button covers it.

## UI contract
`library-grid-controls` renders badges/filters from these fields (pending); home saved_rows + `/api/library` already expose `status/total_ch/read_ch/has_update/last_read_at/added_at`.

Tests: `tests/test_store.cpp` "saved manga stats roundtrip" (schema defaults, update/rollup/clear lifecycle, absent-row no-ops).
