---
type: Finding
title: "'Capped at 5 covers' was a stale candidate plus a title the source does not index"
description: "Report 2026-11-23: Fetch covers showed <=5 alternate covers. Neither host nor plugin caps covers (MangaDex cover API limit=100; Solo Leveling returns 9, Sengoku Komachi Kuroutan 23). Root cause: the pair's stored candidate was frozen at 5 from an older capped fetch, and force could never refresh it because the search used the library name 'Sengoku Komachi Kurou Tan!' (MangaDex returns total 0) instead of the indexed 'Sengoku Komachi Kuroutan'. Fix: bounded title retry chain (stored candidate titles/alt_titles, then scraper detail title/name); orphan gate relaxed so loaded-but-unregistered enrichment plugins are not 'plugin removed'."
tags: [enrichment, mangadex, covers, regression, fixed, orphan-gate]
generated: { by: agent/mcp, at: "2026-11-23T11:07:44Z" }
status: stable
---

## Symptom

"Fetch covers" on a library title showed at most 5 alternate covers.

## What it was not

- Host cap: none (`enrichment_source_response` passes the envelope through).
- Plugin cap: none — `cover?manga[]=<id>&limit=100`; measured live: Solo Leveling 9,
  `a4304d3b-…` (Sengoku Komachi Kuroutan) 23 from the API.

## What it was

1. The stored `title_meta` covers candidate for that pair was written when an older plugin
   capped at 5 (its `fetched_at` is older than the other entries); non-force reads serve it.
2. Refreshing it could never succeed: `enrichment_source_response` searched with
   `saved_manga.name` = "Sengoku Komachi Kurou Tan!", and MangaDex answers `total: 0` for that
   spelling, `total: 1` for "Sengoku Komachi Kuroutan" — every force fell back to the stale 5
   and looked like a cap.

## Fix

- `retry_titles()` in `src/http/enrich_source.cpp`: on a failed match retry a bounded chain —
  titles/alt_titles recorded by earlier enrichment matches, then the scraper detail envelope
  title/name — before the stale fallback. Force now refreshes the candidate (23 covers).
- Orphan gate: a plugin id that is loaded but has no registry row (enrichment sources) is not
  "plugin removed"; `Host::loaded()` closes that hole introduced with library-ownership D7.

## Rule

An enrichment candidate is a cache with provenance: when the UI looks capped, check the stored
`fetched_at` and the query string the host built, not only the fetch code.

# Related Concepts
- [Library ownership: RAM for browse, SQLite for saved, plugin memory derived as orphan (IMPLEMENTED)](../decisions/library-ownership-model.md): Follow-up: the orphan gate must treat loaded-but-unregistered enrichment runtimes as live.
