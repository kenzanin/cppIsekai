---
type: Decision
title: "api_cache db-lock treated as cache miss: local-first regression (mitigated)"
description: "db-lock on api_cache_get was indistinguishable from a cache miss -> call_cached fetched despite a cached row; MITIGATED 2026-11-11 by busy_timeout 3s on ORM + raw opens (1b63e59); unknown-title half was valid_title_pair lowercase-only charset (fixed 4091b5c)."
tags: [regression, sqlite, lock, api-cache, local-first, stale-serve, fixed]
generated: { by: agent/mcp, at: "2026-11-11T13:15:00Z" }
status: stable
---

## Finding (2026-11-11) — MITIGATED same day (commits 1b63e59, 4091b5c)

**Symptom (as found)**: opening a manga from the library still hit the website despite a cached row, and intermittently surfaced unknown-title / error. Log evidence 2026-11-11: `on_pool failed: database is locked` -> `/api/library` 500; `detail failed: database is locked` after two 200 upstream GETs.

**Root cause**: `Store::api_cache_get` (`store_api.cpp`, raw `sqlite3_open` per call) returned `std::nullopt` BOTH on a genuine miss AND when the statement failed with `database is locked`. `call_cached` (`http/stale.hpp`) could not distinguish them, so a lock was treated as a cold miss -> plugin call -> website fetch despite the row existing (local-first violation). The failure-path last-good lookup had the same flaw.

**Why locks happened**: WAL was already on (`store_core.cpp:10`), but raw connections set no `busy_timeout`, so SQLITE_BUSY failed instantly instead of waiting out the concurrent writer (reader position writes, enrichment saves, cover prefetch).

**Fix shipped (1b63e59)**: `busy_timeout(3000)` on the ORM connection (`store_core.cpp`) and `sqlite3_busy_timeout(db, 3000)` on both raw opens in `store_api.cpp`. A locked read now waits instead of masquerading as a miss.

**Residual risk (ponytail)**: a lock that outlives the 3s timeout is still indistinguishable from a miss inside `call_cached`. If it ever reproduces under heavy write load, give the store a miss-vs-error channel (e.g. `expected<optional<ApiCacheRow>, err>`) and make the wrap refuse the network path on error. A drafted `db_err_note` thread-local shim in `stale.hpp` was rolled back — wrong seam; the store layer is where miss-vs-error is knowable.

**Related fix (4091b5c)**: the unknown-title half of the symptom was NOT this bug — `valid_title_pair` (`http/api_ctx.hpp`) had a lowercase-only charset and rejected atsumaru's uppercase ids (`H6P2Z`) with 404 `unknown title`. Charset now case-preserving (A-Z added), traversal guards (`..`, separators) untouched; locked by test_http_enrich `valid_title_pair accepts plugin-minted case, rejects traversal`.

Relates to `architecture/local-first-reads` (violation, now restored) and `decisions/api-cache-null-payload` (same look-aside, different failure mode).
