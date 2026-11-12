---
type: Decision
title: "library-grid-stats: saved_manga stat columns + envelope-derived sync"
description: "saved_manga carries plugin-derived stats (status/total_ch/read_ch/has_update/last_read_at/updated_at); refresh+detail-open sync reuses stale-served envelopes; reader touch clears has_update; read_ch recomputed on open/refresh only via chapters envelope pages — requires plugin to emit pages (atsumaru fixed 2026-11-12; envelope without pages falls back to read>0, causing false Done)."
tags: [store, library, sqlite, sync, has-update, openspec, grid]
generated: { by: agent/cli, at: "2026-11-12T01:03:30Z" }
status: stable
---

Shipped 2026-11-11 (openspec change library-grid-stats, 12/12 tasks, 16/16 ctest). Store-layer implementation of library-page stat design.

Schema (src/store/store_internal.hpp, store_core.cpp):
- saved_manga += status TEXT DEFAULT 'ongoing', total_ch/read_ch/has_update/last_read_at/updated_at INTEGER DEFAULT 0 (unix seconds).
- Boot migration: pragma.table_info(saved_manga) guard + ALTER TABLE ADD COLUMN in Store::Store.

Store helpers (src/store/store_search.cpp):
- saved_stats_update(plugin,slug,status,total_ch): has_update=1 iff total_ch grows; updated_at bumps only on real change; empty status keeps stored one.
- saved_read_rollup(plugin,slug,read_ch,ts): ts==0 leaves last_read_at untouched.
- saved_clear_update(plugin,slug): reader touch -- stamps last_read_at=now ALWAYS, clears has_update=0.
- saved_signal_update(plugin,slug,synopsis,alt_titles): library-dedup signal sync (empty args untouched).

Sync hooks (2026-11-12): sync_saved_stats called on /api/refresh POST and detail-open stale-serve; reuses stale-served envelopes, no double fetch. read_ch recomputed by walking chapters envelope on refresh/detail-open (host read/<id> >= page count, fallback >0). Reader position POST clears has_update + stamps last_read_at. Live smoke 2026-11-12: refresh all 10 saved titles -> total/read/status populated (mangadex a4304d3b: 57/100 read); /reader/position cleared o90AN has_update + stamped last_read_at.

Badge semantics clarification 2026-11-12 (user): New = has_update=1; Done = read>=total>0 (all chapters read); Complete = plugin status 'Completed' (case-insensitive — atsumaru 'Completed', mangadex 'ongoing'). Status from plugin detail envelope, never normalized by host.

# Related Concepts
- [Library page design (status, progress, done/new badges)](library-page.md): library-grid-stats stores and syncs the stat columns that library-page badges and filters render
