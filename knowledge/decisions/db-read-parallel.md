---
type: decision
title: "DB read-parallel deferred: keep single-writer db_pool; read-only scans on demand"
description: "DB concurrency decision (2026-11-11, deferred): all reads/writes ride db_pool=1 thread today (sqlite_orm storage_t not thread-safe; single ORM connection). Proposed: option 3 — heavy read-only scans (e.g. library dedup FTS5) open SQLITE_OPEN_READONLY raw connections on demand instead of refactoring Store into a connection pool. Full read-parallel option 1 (N reader conns + 1 writer, WAL multi-reader/single-writer) is the harder path and explicitly deferred per user; revisit if grid/dedup queries become hot. Write path stays single-writer always."
generated: { by: agent/mcp, at: "2026-11-11T15:54:23Z" }
status: stable
---


