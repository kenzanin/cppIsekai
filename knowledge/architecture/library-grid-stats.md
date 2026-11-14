---
type: Decision
title: "library-grid-stats: saved_manga stat columns + envelope-derived sync"
description: "saved_manga stat columns (status/total_ch/read_ch/last_read_at/updated_at) plus the status_flags bitfield (bit0=is_new, bit1=is_done, bit2-5 one-hot status category, bit6=is_dim) persisted per-title; detail open and the chapters-only scheduler both write it through compose_flags; has_update was dropped in status-flags-followup-cleanup (bit0 is the single New source)."
tags: [store, library, sqlite, sync, status-flags, openspec, grid]
generated: { by: agent/mcp, at: "2026-11-14T10:18:02Z" }
status: stable
---

Shipped 2026-11-11 (openspec change library-grid-stats, 12/12 tasks, 16/16 ctest). Store-layer implementation of library-page stat design.

Schema (src/store/store_internal.hpp, store_core.cpp):
- saved_manga += status TEXT DEFAULT 'ongoing', total_ch/read_ch/has_update/last_read_at/updated_at INTEGER DEFAULT 0 (unix seconds), status_flags INTEGER DEFAULT 0 (bit0=is_new, bit1=is_done).
- Boot migration: pragma.table_info(saved_manga) guard + ALTER TABLE ADD COLUMN in Store::Store (additive only; future status bits reuse status_flags without migration).
- SavedMangaEntity / SavedManga struct gain status_flags; saved_row() maps it; save_manga() preserves existing flags on plain re-save.

Store helpers (src/store/store_search.cpp):
- saved_stats_update(plugin,slug,status,total_ch): has_update=1 iff total_ch grows; updated_at bumps only on real change; empty status keeps stored one.
- saved_read_rollup(plugin,slug,read_ch,ts): ts==0 leaves last_read_at untouched. Computes status_flags = (done?2:0)|(news?1:0) where done = total_ch>0 && read_ch>=total_ch, news = has_update!=0 && !done.
- saved_clear_update(plugin,slug): reader touch -- stamps last_read_at=now ALWAYS, clears has_update=0.
- saved_signal_update(plugin,slug,synopsis,alt_titles): library-dedup signal sync (empty args untouched).

Sync hooks (2026-11-12): sync_saved_stats called on /api/refresh POST and detail-open stale-serve; reuses stale-served envelopes, no double fetch. read_ch recomputed by walking chapters envelope on refresh/detail-open (host read/<id> >= page count, fallback >0). Reader position POST clears has_update + stamps last_read_at.

Badge semantics (client reads stored flags): New = bit0 of status_flags; Done = bit1 of status_flags; Done wins over New when both conditions computed (done=true forces news=false in rollup). Complete = plugin status 'Completed' (case-insensitive). Status from plugin detail envelope, never normalized by host.

Server output: /api/library and library page render (http_api_library.cpp, http_pages.cpp) include status_flags per saved row. Grid template (grid_body.mustache) renders data-status-flags and reads bit0/bit1 via badge(); detail page (detail.mustache) reads the same stored bits via detail.status_flags, injected into the detail envelope by /api/detail (api_ctx.hpp inject_status_flags) after the rollup runs. Both pages therefore read the one stored row: detail open is the writer, neither page derives badges client-side.

2026-11-14 fix (c0fce4e): saved_list()/saved_get() were not mapping the status_flags column into SavedManga, so the bitfield always read 0 and no badge could render in the grid; the detail page had also been computing New/Done client-side from the reader hint and chapter state, a second signal that drifted from the stored row. Now both map the column, /api/detail echoes the stored flags, detail badges read detail.status_flags, and saved_clear_update clears bit0 on a reader touch.

# Related Concepts
- [Library page design (status, progress, done/new badges)](library-page.md): library-grid-stats stores and syncs the stat columns that library-page badges and filters render
