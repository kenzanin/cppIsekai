# Proposal: library-ownership

## Why

Three ownership bugs sit behind one question: who is the master of manga data?

1. **SQLite holds data nobody saved.** Opening a title from search materializes `chapters`,
   `chapter_pages`, `reader_positions` and `title_meta` immediately. The live database measured
   33,933 chapter rows for 371 pairs while only 121 pairs are saved — 24,862 rows (73%) belong to
   titles the user never kept.
2. **Removing a plugin deletes the user's library.** `plugin_prune_absent` drops `saved_manga`,
   `chapters`, `reader_positions`, `title_meta` and `update_attempts` for an absent plugin id.
   Deleting a plugin folder (or swapping a Lua implementation for a JS/WASM one under a different
   folder id) destroys reading history that took months to build.
3. **Pre-save reading is second class.** Progress made while browsing is written to the same tables
   as saved data, so it can only be discarded by deleting library-shaped rows.

## What Changes

- **Browse data becomes ephemeral.** For a pair with no `saved_manga` row, chapter identities,
  flags, `read_max`, page URLs, reader position, enrichment rows and the per-pair host fields live in
  an in-process map inside the store, not in SQLite. A daemon restart empties it — no orphans can be
  produced by construction.
- **Store routes per pair, callers unchanged.** Every pair-keyed store method checks "is this pair
  saved?" and picks SQLite or the map. Routes, reader, prefetch worker and export keep their current
  signatures.
- **Save promotes the browse state.** `POST /api/library` moves the pair from the map into SQLite in
  one transaction — including the chapter flags / `read_max` / position / chosen enrichment the user
  produced *before* saving. Reading 5 chapters unsaved and then adding the title keeps those 5 marks.
- **Plugin removal keeps the library.** A scan that finds a registered plugin id without a folder now
  GCs only its caches and registry row (`title`, `api_cache`, `plugin_state`, `plugin`) and leaves the
  saved pair and its children untouched. Those entries become **orphans**: served from local rows,
  never calling the plugin, recoverable the moment a plugin with the same id is installed again.
- **Orphan is derived, not stored.** The state (live / offline / orphan) comes from the plugin registry
  at read time, so reinstalling revives entries with zero writes and no migration.
- **Orphans stay manageable.** `GET /api/library` reports the state, the grid shows a badge and filter,
  migration works unchanged, and `POST /api/library/purge-orphans` wipes them (rows + disk images)
  when the user really wants them gone.
- **Legacy cleanup.** The dead `title` table and `store.search/count` paths go; the per-pair
  `plugin_state` families (`host/chapters_fp`, `host/novel`, `host/detail_url`) become `saved_manga`
  columns; `host/pos` (zero readers) goes.

## What does not change

Plugin registry/picker behavior, the ABI and envelopes, `api_cache` (it caches per-request envelopes,
including searches, so it is not part of the pair tree and keeps its own prune), the reader window
math, image cache layout, dedup/FTS, backup/maintenance.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save`: browse data is ephemeral; saving promotes it; plugin removal retains entries as
  orphans; orphans are visible, filterable and purgeable.
- `chapter-store`: chapter rows persist only for saved pairs; unsaved pairs hold the same data in
  memory and hand it over on save.
- `plugin-system`: removing a plugin folder no longer deletes library data.
