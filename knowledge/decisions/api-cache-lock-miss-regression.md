---
type: Decision
title: "api_cache db-lock treated as cache miss: local-first regression (UNFIXED)"
description: "UNFIXED: database-is-locked on api_cache_get is indistinguishable from a cache miss, so call_cached takes the network path despite a cached row (local-first violation) and last-good stale-serve fails; WAL is on but raw connections lack busy_timeout."
tags: [regression, sqlite, lock, api-cache, local-first, stale-serve, unfixed]
generated: { by: agent/mcp, at: "2026-11-11T11:55:30Z" }
status: draft
---

## Finding (2026-11-11, UNFIXED)

**Symptom**: opening a manga from the library still hits the website, and intermittently shows unknown-title / error even though the row is cached. Daemon log 2026-11-11: `16:34:45 on_pool failed: database is locked` -> `/api/library` 500; `16:38:55 [plugin:mangadex] detail failed: database is locked` after two 200 upstream GETs.

**Root cause**: `Store::api_cache_get` (`src/store/store_api.cpp`, raw `sqlite3_open` per call) returns `std::nullopt` BOTH on a genuine miss AND when the statement fails with `database is locked`. `call_cached` (`src/http/stale.hpp`) has no way to distinguish them, so a lock is treated as a cold miss -> plugin call -> website fetch despite the row existing. The failure-path last-good lookup has the same flaw: lock -> no last-good -> error envelope reaches the UI.

**Why locks happen**: WAL is already on (`store_core.cpp:10` `journal_mode(WAL)`), but the raw connections set no `busy_timeout`, and writes (reader position, enrichment saves, cover prefetch) contend with reads; SQLITE_BUSY fails instantly instead of waiting.

**Planned fix (minimal)**:
1. `PRAGMA busy_timeout` (e.g. 3000ms) on every raw connection open in `store_api.cpp` (and ideally the one-shot opens in `store_core.cpp`).
2. Lock-aware guard in `call_cached`: if `api_cache_get` fails due to lock (not row-absence) the wrap must NOT take the network path — serve error-fallback or last-good, never silently fetch.

**Rolled-back attempt**: a `db_err_note/db_err_take` thread-local shim in `stale.hpp` was drafted and reverted — wrong seam; the store layer is where miss-vs-error is knowable (e.g. return `expected<optional<ApiCacheRow>, err>` or a `bool last_error` flag).

Relates to `architecture/local-first-reads` (violation) and `decisions/api-cache-null-payload` (same look-aside, different failure mode: NULL payload vs locked read).

# Related Concepts
- [Library reads are local-first](../architecture/local-first-reads.md): The db-lock-as-cache-miss bug violates the local-first read contract
