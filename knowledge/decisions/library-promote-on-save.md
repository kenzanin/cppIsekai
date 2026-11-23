---
type: Decision
title: "Library promotion: api_cache is the staging store; chapters become a read source (planned, openspec library-promote-on-save)"
description: "IMPLEMENTED (branch library-promote-on-save, 5d6af9d + unsave-purge): POST /api/library promotes cached detail+chapters onto saved_manga/chapters rows via the existing materializers (api_cache = the staging store, no new table); /api/chapters row-served via chapters.release_at; D5 revised: unsave now WIPES the pair (satellites+api_cache via json_extract+disk images via Cache::purge_pair) so re-add equals a fresh add; no FKs, plugin GC + prune unchanged."
tags: [library, saved_manga, api_cache, chapter-store, local-first, openspec, planned]
generated: { by: agent/mcp, at: "2026-11-23T06:12:41Z" }
status: stable
---

## Decision

Promotion from the temp store to the library row happens **at add time**, and the temp store is the
existing `api_cache` JSONB read model — not a new staging table and not a JSON1 column.

- `POST /api/library` (db task): `save_manga` first, then `materialize_detail` (after `apply_chosen`),
  `materialize_chapters`, `sync_saved_stats` from the cached `detail`/`chapters` rows.
- `GET /api/chapters` answers from `chapters` rows when they exist (`cached:true`), so the
  `cache_prune_expired` cliff (ttl + 7d) no longer decides whether a library open hits the network.
- `chapters.release_at` verbatim (epoch/ISO) so the row-built envelope is not lossy; joins the C3 seed
  fingerprint.
- Master pair = `saved_manga` + `chapters`(`flags` + `read_max`) + `chapter_pages` +
  `reader_positions` + `title_meta` + `update_attempts` + pair-scoped `plugin_state` keys.
  **Unsave wipes the entire pair** (one txn + raw `api_cache` delete + `Cache::purge_pair` disk
  sweep) — re-add == fresh add from search. No FKs; plugin GC + prune unchanged.
- The host/skip|read|total shadow keys are **retired**: read progress lives on
  `chapters.read_max` (`chapter_read_max_bump` flips the read bit at the pages count), the state
  endpoint reads flags/read_max/pages table, and a boot pass backfills read_max from old shadows
  before dropping them. Single source of truth, nothing pair-less left.

## Rejected alternatives

- New temp table / JSON1 column: duplicates `api_cache`.
- Exempt saved titles from the api_cache prune: join over unindexed `params_json`.
- Eager plugin fetch at save: needless network; the envelope is already local.
- FK/cascade schema: composite-string keys — the cascade is one explicit store op.
- Keeping the shadow keys as compat: two write paths for one fact; dead after the flags migration.

## Files

- `openspec/changes/library-promote-on-save/{proposal,design,tasks}.md`
- Deltas: `specs/{library-save,chapter-store,manga-detail-page}/spec.md`
- Commits: 5d6af9d promote+row-serve, e4147f1 unsave purge, 34f7a25 shadow sweep, shadow retirement
  (read_max) this change
