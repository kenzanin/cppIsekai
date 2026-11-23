# Proposal: library-promote-on-save

## Why

The flow the project markets as local-first is `search -> detail -> add -> open`, and it already has
the right shape: every plugin envelope lands in `api_cache` as a JSONB row (`search-cache`), and a
saved row is served ahead of the plugin (`manga-detail-table`). Two holes make the "library open
never touches the plugin" claim depend on luck:

- **Promotion is lazy.** `POST /api/library` stores only `name`/`cover`. The display detail
  (`authors`, `artists`, `tags`, `synopsis_display`) reaches `saved_manga` on the *next* detail
  open, when `saved_detail_response` misses and the route runs `materialize_detail`. A title saved
  and never reopened is never promoted.
- **Chapters are never promoted at all.** `/api/chapters` always goes through `call_cached`; the
  `chapters` rows written on every open are a flag/bookkeeping store, not a read source. So the
  chapters list dies with the cache row — and `cache_prune_expired` drops `api_cache` rows at
  `fetched_at + ttl + 7d` (24h TTL for detail/chapters, `scheduler.cpp`). After ~8 days an unmaterialized
  save, or any saved title's chapter list, goes back to the network.

The tempting fix — a new staging table, or a JSON1 column holding the raw envelope — duplicates a
store that already exists. `api_cache` *is* the temp store; `chapters` *is* the canonical chapter
list; `saved_manga` *is* the library row. Nothing needs to be invented, only wired.

## What Changes

- **Add-to-library promotes what is already cached.** In the same db task as the save, the route
  reads the title's `api_cache` detail/chapters rows (the keys the preceding open minted) and runs
  the existing `materialize_detail` / `materialize_chapters` / `sync_saved_stats` path. No new
  table, no new column, no new parser, no plugin call. Idempotent; a no-op when nothing is cached.
- **Chapters get a row-served read path.** `GET /api/chapters` answers from `chapters` rows when they
  exist, building the same envelope a parse produces, flagged `cached:true`. This removes the
  `api_cache` prune cliff for saved titles; unsaved titles and cold misses keep the current
  cached-or-live behavior.
- **`chapters.release_at`** is stored verbatim (epoch or ISO text) so the row-built envelope is not
  lossy — the detail page renders `ch.release_at`. Additive migration, plus it joins the C3 seed
  fingerprint so a release-date-only change still refreshes.
- **Ownership is stated, not enforced.** `saved_manga` stays the master of *user state*;
  `chapters`, `chapter_pages`, `reader_positions`, `title_meta`, `update_attempts`, `plugin_state`,
  `api_cache` stay identity-keyed caches with no FK to the saved row and no cascade on unsave
  (re-adding is instant; plugin removal already GCs by plugin id). No cascade migration.

## What does not change

Plugin ABI and envelopes, cache layout, the reader window/position logic, `search-cache` TTL/prune
behavior, `title_meta` provenance, and the FTS/dup pipeline. `pages` stays `api_cache`-only on
purpose: signed page URLs must be refreshed, never archived.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save`: adding a manga promotes its cached detail and chapters onto local rows.
- `chapter-store`: `release_at` joins the chapter identity; the chapter list is served from rows.
- `manga-detail-page`: a saved manga's detail and chapters are both answered locally, independent of
  `api_cache` age.
