# Proposal: manga-detail-table

## Why

A saved manga's display metadata is resolved fresh on every detail open: the plugin envelope is
cached raw in `api_cache`, `title_meta.chosen` overrides it, and `apply_chosen` merges the two in
memory. Nothing durable holds the result. `saved_manga` carries only `name`, `cover`, `synopsis`
and `status_flags`; authors, artists and tags exist nowhere outside the raw envelope.

Consequences today:

- Every page that wants detail fields re-runs the merge, and only the detail API knows how.
- A plugin that returns an empty or reshaped payload silently blanks a saved entry's metadata
  even though the library row is the thing the user keeps (the 2026-11-15 empty-`name` bug was
  exactly this: the resolver ran but the result was never stored).
- Library cards, detail h1 and any future page derive the display name from three different
  fallbacks, so they can disagree.

## What Changes

- **`saved_manga` becomes the source of truth for a saved manga's display detail.** The row gains
  `authors`, `artists`, `tags` (joining the existing `name`, `cover`, `synopsis`, `status`), so one
  row holds everything the detail page renders.
- **Write-through on resolve.** When the detail API resolves a saved manga's envelope (chosen
  overrides + name resolver), the resolved values are persisted onto that row. Enrichment
  candidates keep living in `title_meta` only.
- **Read-first.** For a saved manga the detail API answers from the stored row and does not call
  the plugin; `POST /api/refresh` (the ⟳ Update button) is the path that re-asks the plugin and
  rewrites the row. Titles that are not saved keep today's live + stale-serve behaviour.
- **One name everywhere.** Library card, detail h1 and search-cache cards read the same stored
  name, so the 2026-11-15 mismatch class cannot come back.

## What does not change

The plugin ABI (`detail`/`chapters`/`pages` envelopes), `api_cache` shape, the chapter list,
reader window/position logic and the cache layout are untouched. `title_meta` stays the provenance
store for candidates and the user's `chosen`; it is not replaced. Chapters stay live/stale-served —
only metadata becomes durable.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save`: the saved row gains display-detail columns and every reader of a saved manga's
  metadata reads them.
- `manga-detail-page`: the detail API serves a saved manga's stored detail and only calls the
  plugin on explicit refresh or cold miss.
- `detail-enrichment-fields`: the persistence requirement narrows — candidates never write
  `saved_manga`, but the resolved display detail does.

## Dependencies

None. This change is additive on the store and can land before or after
`openspec/changes/refactor-hygiene`; it touches `http_api_title.cpp` and `enrich_internal.hpp`,
which that change also touches, so the two should not be developed in parallel worktrees.

## Impact

- `src/store/` — entity columns, additive migration, accessors.
- `src/http/http_api_title.cpp`, `src/http/enrich_internal.hpp` — write-through + read-first.
- `src/http/http_pages.cpp` — library card name from the stored row.
- `templates/detail.*` — hydrate from the stored detail when present.
- `tests/test_store.cpp`, `tests/test_http_enrich.cpp` — new expectations.

## Out of scope

- Storing chapters or pages (they stay cache-only).
- Backfilling detail for saved rows that were never opened: the first open after the migration
  resolves and stores them.
- Cross-plugin detail sharing (a MangaDex enrichment result for a mangafreak row) — the row is
  keyed `(plugin, slug)` as today.
