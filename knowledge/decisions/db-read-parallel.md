---
type: decision
title: "DB read-parallel deferred: keep single-writer db_pool; read-only scans on demand"
description: "DB concurrency decision (2026-11-11, deferred): all reads/writes ride db_pool=1 thread today (sqlite_orm storage_t not thread-safe; single ORM connection). Proposed: option 3 — heavy read-only scans (e.g. library dedup FTS5) open SQLITE_OPEN_READONLY raw connections on demand instead of refactoring Store into a connection pool. Full read-parallel option 1 (N reader conns + 1 writer, WAL multi-reader/single-writer) is the harder path and explicitly deferred per user; revisit if grid/dedup queries become hot. Write path stays single-writer always."
tags: [sqlite, thread-pool, on_pool, performance, graph-audit]
generated: { by: agent/mcp, at: "2026-11-24T08:07:08Z" }
status: stable
---

## 2026-11-24 graph audit + on_pool batching A/B

Fresh codebase-memory graph (14,983 nodes / 49,688 edges; generation 2026-11-24T07:49Z, coverage clean on cited paths) found 75 on_pool call sites in src/http and high cognitive complexity in enrichment_source_response (105), apply_chosen (85), normalize_chosen (72), browser::call (62), Cache::Cache (29), img_ensure (24). saved_exists inbound fan-in 23 but is already a single indexed EXISTS under OpTrace; cache constructor is boot-only; browser call is Chrome-bound - left alone.

Applied: P0 batch 2-3 DB micro-ops per on_pool lambda in apply_read_rollup, sync_saved_stats, refresh_title; P1 split enrichment_source_response into 5 local helpers (error sanitize, cached-source lookup, fetched merge, alt-title normalization, ok response) without behavior change.

A/B with identical restart + 24 warm curl samples (first 4 discarded), same daemon and DB:

| route | baseline median | batched median |
|---|---|---|
| /api/detail/1manga/sengoku-komachi-kurou-tan | 0.0072 s | 0.0075 s |
| /api/chapters/1manga/sengoku-komachi-kurou-tan | 0.0031 s | 0.0030 s |

Difference is inside noise - batching removes promise/future allocations but these paths are already dominated by envelope parsing and one serialized sqlite handle. Verdict: keep the batching as a small tidy-up, do NOT claim a speed win; do not start a wider on_pool rewrite without a profiler showing dispatch cost. Runtime log since last boot: 0 slow ops, 0 lock errors. Revisit only if instrumented on_pool dispatch time becomes material.
