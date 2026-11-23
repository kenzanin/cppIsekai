---
type: Decision
title: "Library ownership: RAM for browse, SQLite for saved, plugin memory marked orphan (planned, openspec library-ownership)"
description: "PLANNED 2026-11-23: browse-only pairs hold chapters/flags/read_max/pages/position/title_meta in an in-process map (nothing persistent, restart clears it); POST /api/library promotes that state into SQLite including pre-save read marks; plugin folder removal GCs only caches+registry so saved entries survive as orphans derived from the registry (reinstall revives, purge route removes). Measured motivation: 24,862 of 33,933 chapter rows (73%) belonged to never-saved pairs."
tags: [library, store, saved_manga, orphan, ephemeral, plugin-lifecycle, openspec, planned]
generated: { by: agent/mcp, at: "2026-11-23T08:33:36Z" }
status: stable
---

## Model

Two hierarchies, each with its own root — conflating them was the old confusion:

- **plugin** = registry/namespace root (id, version, enabled, runtime). Reinstalling revives data.
- **saved_manga** = user-data root. Its children are `chapters` (flags, read_max),
  `chapter_pages`, `reader_positions`, `title_meta`, `update_attempts` and the per-pair host fields
  (`chapters_fp`, `novel`, `detail_url`).

## Rules

1. A pair with no `saved_manga` row writes **nothing** to SQLite; browse state lives in an in-process
   map and dies with the process. No orphans possible by construction.
2. Saving promotes that map into SQLite in one transaction — chapter flags and read_max produced before
   the save ride along ("read 5 chapters, then save" keeps the 5 marks).
3. Unsave wipes the pair (rows, `api_cache` envelopes, disk images).
4. Plugin folder removal GCs caches + registry only (`title`, `api_cache`, `plugin_state`, `plugin`).
   Library rows are retained and reported as **orphan**, derived from the registry — never stored.
5. Orphan = no upstream: behaves exactly like a disabled plugin (local rows, no calls, no scheduler
   picks). Reinstall with the same plugin id revives it with zero writes.
6. `api_cache` stays in SQLite: its key is a request shape (search rows have no manga identity), so it
   cannot be a child of the library tree; it keeps its own prune.
7. `plugin_state` after this change holds only plugin-owned global keys.

## Rejected

- Stored `orphan` bit: needs writers on install/removal + repair on drift; registry is the single
  source of truth. Bit7 of `status_flags` remains the escape hatch if a cheap sortable query is needed.
- FK cascade from satellites to `saved_manga`: forces a parent row for browse-only titles, breaking
  read-before-save, and needs a table rebuild migration.
- SQLite TEMP TABLE for browse data: same disappearance guarantee, more DDL/SQL for shapes that already
  have typed structs.
- Backfilling legacy orphans into the new model: not needed — the ephemeral store stops the source of
  new ones and the purge route clears the old ones.

## Known trade-offs

- Unsaved reading progress is lost on daemon restart (accepted; upgrade path = persist only
  `reader_positions` for unsaved pairs).
- Image cache namespaces key on the plugin's **display name**, so an implementation swap that renames
  the plugin loses cached image bytes (refetchable); library data is untouched.

# Related Concepts
- [Library promotion: api_cache is the staging store; chapters become a read source (planned, openspec library-promote-on-save)](library-promote-on-save.md): Supersedes the where-to-stage question: the staging store is process memory, not api_cache, and promotion carries pre-save read state.
