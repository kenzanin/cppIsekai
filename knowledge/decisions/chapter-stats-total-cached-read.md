---
type: decision
title: "Chapter rows: total/cached/read page counters"
description: "Chapter rows render total/cached/read; totals are learned host-side on first read (host/total state keys), cached comes from a cache-tree walk - no plugin changes."
tags: [reader, chapter-state, cache, plugin-state, ui]
generated: { by: agent/mcp, at: "2026-11-15T16:28:04Z" }
status: stable
---

## Context
Chapter list rows could only show the read-marker count; sites do not expose per-chapter page counts in their chapter listings (verified against mangafreak markup: table columns are Title/Timeline/Bookmark/Download only).

## Decision
- **total**: learned on read. Every `GET /api/pages/<plugin>/<slug>/<ch>` persists `host/total/<ch_id> = pages.size()` (src/http/http_api_content.cpp, glaze `envelope_data` - NOT crow json, the envelope is `{ok,data:{pages:[...]}}`). One plugin_state row per chapter; unknown until first read -> `-`.
- **cached**: `POST /chapter/state` now takes `{plugin,slug,ids}` and walks `data/cache/img/<ns>/<slug>/ch*` once; count = files in the `chNNN-<last8of id>` dir. Legacy number-only dirs are not reported.
- **read**: unchanged `host/read/<id>` marker.
- Row renders `total/cached/read` with `-` for unknown, hover title "total page/cached/read". `isRead` gates on persisted total, falling back to plugin `pages` field, then read>0.
- Host-side only; plugins unchanged (host is source of truth, per constraints/plugin-chapter-number-placeholders lineage).

## Consequences
- Chapters never opened show `-/-/0`; totals fill in as the library is read.
- `kTotalPrefix` = `host/total/` in src/http/api_ctx.hpp alongside skip/read.
- Landed fab8959 (after 6e589c0 status normalization); suite 18/18; live: ch1 => {cached:61,total:67,read:51}.
