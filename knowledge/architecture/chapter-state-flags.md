---
type: Decision
title: "Chapter state: one flags bitfield per chapter row, chapters as metadata hub"
description: "chapters(plugin, slug, chapter_id, number, title, url, flags, updated_at) is the hub with bit0=read/bit1=hidden; chapter_pages is its child; POST /chapter/flags toggles by ids or all; hidden ids are also stepped over by /api/pages prev/next."
tags: [chapter-state, flags, bitfield, store, sqlite, reader, detail-page]
generated: { by: agent/mcp, at: "2026-11-24T10:40:30Z" }
status: stable
---

Landed as OpenSpec change `chapter-state` (archived 2026-11-18 under `openspec/changes/archive/2026-11-18-chapter-state`).

## Decisions
- **D1** `chapters.flags INTEGER` per row: bit0 = read, bit1 = hidden, rest reserved. Same pattern as `saved_manga.status_flags` (see architecture/status-flags-bitfield). A manga-wide bitmap keyed by list position was rejected: chapter numbers are non-unique doubles (24a/24b, 17.1, 0), so position bits silently corrupt state whenever a refresh inserts a chapter.
- **D2** Key is `(plugin, slug, chapter_id)`; rows persist at flags=0 so a later toggle never has to recreate identity. Legacy-key backfill only on a unique match.
- **D3** One generic route `POST /chapter/flags {ids|all, set, clear}`. "all", "range 10..1" and "previous N" are computed **client-side** from the chapter list the page already holds -- parity with Suwayomi's `updateChapters`, whose server is equally dumb.
- **D4** `chapter_pages(chapter_id, idx, url)` is replaced per pages parse; page *images* stay on the filesystem (the transcode ext is already indexed by the boot scan) -- DB never mirrors file paths.
- **D5** The read bit is the single source of truth. The host auto-sets it when the **last** page is served (threshold `COUNT(chapter_pages)`), and the `read_ch` rollup derives from the bits. tests/test_http_enrich.cpp was rewritten to this contract.
- **D6** Reader skip filter and `/chapter/state` moved onto the store helper in the same change; no dual-read window.

## Implementation notes that bit
- sqlite_orm: `insert()` on a table with a composite PK plus `default_value` columns binds NULL -- use `st.replace` (house pattern). Symptom was valid entities inserting as empty rows.
- `in(...)` in sqlite_orm 1.9 needs a plain member pointer: `in(&Chapter::chapter_id, ids)`, not `in(c(...), ids)`.
- Bulk toggle cannot use bitwise operators inside `update_all`'s set-clause: select -> XOR/AND in C++ -> batch replace, inside one OpTrace.
- Materialization hook lives in the canonical UI path (`title_data_response` chapters branch -> `materialize_chapters` in src/http/api_ctx.hpp); hooking only `apply_read_rollup` left the table empty because `/api/chapters` never passes through it.
- Pages/images: read bits verified both ways live (page 19 auto-sets, page 5 does not disturb it).

## Hidden (bit1) is a reader-navigation flag, not just a list filter
The chapter list already filtered `kSkip` rows, but `/api/pages` still computed prev/next from the raw number-sorted order, so the reader walked straight into hidden chapters. Fixed 2026-11-24: `PagesCtx.hidden` (set of skip-flagged chapter ids, filled from `Store::chapter_flags_list`) is stepped over when resolving prev/next in `build_pages_response` (src/http/pages_api.cpp) -- both directions, so 1 -> 3 when 2 is hidden and 3 -> 1 back. A hidden chapter opened directly still serves its own pages; only the automatic walk skips it. reader::window_parts already honored `Skipped`, so the prefetch window and `/api/pages` now agree. Live-verified in the browser: with the duplicate release hidden, PageDown at the last page jumped 32 -> ch 33 instead of the hidden id; unhiding restored 32 -> 32b.

## UI
Detail page: header `✓ all` / `clear` (clear is confirm-guarded) plus a per-row `↑✓ mark-prev` that posts a single range update for every chapter at or before the picked one.

# Related Concepts
- [status_flags bitfield: bit0-6 layout + single compose_flags writer](status-flags-bitfield.md): Chapter flags follow the same bitfield convention as saved_manga.status_flags.
- [Plugin chapter numbers are untrusted: order, duplicates and placeholder 0](../constraints/plugin-chapter-number-placeholders.md): Why manga-wide position bitmaps were rejected: chapter numbers are untrusted, duplicated and non-unique.
