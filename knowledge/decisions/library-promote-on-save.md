---
type: Decision
title: "Library promotion: api_cache is the staging store; chapters become a read source (planned, openspec library-promote-on-save)"
description: "IMPLEMENTED (branch library-promote-on-save, 5d6af9d + unsave-purge): POST /api/library promotes cached detail+chapters onto saved_manga/chapters rows via the existing materializers (api_cache = the staging store, no new table); /api/chapters row-served via chapters.release_at; D5 revised: unsave now WIPES the pair (satellites+api_cache via json_extract+disk images via Cache::purge_pair) so re-add equals a fresh add; no FKs, plugin GC + prune unchanged."
tags: [library, saved_manga, api_cache, chapter-store, local-first, openspec, planned]
generated: { by: agent/mcp, at: "2026-11-23T05:20:33Z" }
status: stable
---

## Decision

Promotion from the temp store to the library row happens **at add time**, and the temp store is the
existing `api_cache` JSONB read model — not a new staging table and not a JSON1 column.

- `POST /api/library` (db task): `save_manga` first, then `materialize_detail` (after `apply_chosen`),
  `materialize_chapters`, `sync_saved_stats` from the cached `detail`/`chapters` rows.
- `GET /api/chapters` answers from `chapters` rows when they exist (`cached:true`), so the
  `cache_prune_expired` cliff (ttl + 7d) no longer decides whether a library open hits the network.
- `chapters.release_at` added verbatim (epoch or ISO) so the row-built envelope is not lossy; it also
  joins the C3 seed fingerprint.
- `saved_manga` is master of *user state* (name/cover/stats/flags/pin/display detail). Satellites
  (`chapters`, `chapter_pages`, `reader_positions`, `title_meta`, `update_attempts`, `plugin_state`,
  `api_cache`) stay identity-keyed caches with no FK and no cascade on unsave.

## Why

The open that precedes a save already wrote both envelopes to `api_cache`; the only missing piece was
wiring the existing materializers into the save path. A staging table would duplicate that store,
its prune scheduler and its lifecycle. Chapters needed a read source, and the canonical per-chapter
rows already existed.

## Rejected alternatives

- New temp table / JSON1 column: duplicates `api_cache`.
- Exempt saved titles from the api_cache prune: needs a join over unindexed `params_json`.
- Eager plugin fetch at save: needless network; the cached envelope is already local.
- FK/cascade to `saved_manga`: composite-string keys, sqlite_orm cannot ALTER cheaply, and orphan
  satellites are desirable (re-add is instant). Plugin removal already GCs per plugin.

## Files

- `openspec/changes/library-promote-on-save/{proposal,design,tasks}.md`
- Deltas: `specs/{library-save,chapter-store,manga-detail-page}/spec.md`

# Related Concepts
- [Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)](saved-manga-detail-source-of-truth.md): Extends it: the detail row is now materialized at save time instead of on the first open after save.
- [Library reads are local-first](../architecture/local-first-reads.md): Closes the gap where a saved title's chapters still depended on api_cache age for local-first reads.
- [api_cache NULL payload served as 200-empty-body](api-cache-null-payload.md): Promotion treats a NULL/empty payload row as nothing cached (no-op), same miss rule.
