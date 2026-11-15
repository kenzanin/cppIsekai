# Proposal: manga-detail-table

## Why

A saved manga's display metadata is resolved fresh on every detail open: the plugin envelope is
cached raw in `api_cache`, `title_meta.chosen` overrides it, and `apply_chosen` merges the two in
memory. Nothing durable holds the result. `saved_manga` carries only `name`, `cover`, `synopsis`
(plugin-source) and `status_flags`; authors, artists and tags exist nowhere outside the raw
envelope.

Consequences today:

- Every page that wants detail fields re-runs the merge, and only the detail API knows how.
- A plugin that returns an empty or reshaped payload silently blanks a saved entry's metadata even
  though the library row is the thing the user keeps (the 2026-11-15 empty-`name` bug was exactly
  this: the resolver ran but nothing stored its output).
- Library cards and the detail heading derive the display name from different fallbacks, so they can
  disagree.

## What Changes

- **`saved_manga` becomes the source of truth for a saved manga's display detail.** New columns
  `authors`, `artists`, `tags`, `synopsis_display` join `name`, `cover`, `status`. The plugin-source
  `synopsis`/`alt_titles` stay untouched (duplicate clustering reads them).
- **Write-through at the route.** When `/api/detail` or a full refresh resolves a saved manga's
  envelope (chosen overrides + name resolver), the resolved values are persisted to the row.
  Enrichment candidates and `chosen` still live only in `title_meta`; `apply_chosen` stays a pure
  merge.
- **Read-first.** A saved row with stored detail answers `/api/detail` without a plugin call, and
  the response is flagged `cached` because it is the offline copy. `POST /api/refresh` (⟳ Update)
  re-asks the plugin and rewrites the row. Titles that are not saved keep today's live +
  stale-serve behaviour.
- **Reset keeps working.** `Reset detail` / `Reset cover` clear the materialized fields so the next
  open re-resolves them.
- **Enrichment UI keeps working.** A row-served response still carries `enrichment` and
  `chosen_cover`, so the picker hydrates exactly as today.
- **Library and detail share one name.** Both read the stored row. Search-result cards keep reading
  the `title` search cache — the guarantee is scoped, not global.

## What does not change

The plugin ABI (`detail`/`chapters`/`pages` envelopes), `api_cache` shape, the chapter list, reader
window/position logic and the cache layout are untouched. `title_meta` stays the provenance store for
candidates and `chosen`. Chapters stay live/stale-served — only metadata becomes durable.
Auto-update stays `chapters_only` (no extra network); ⟳ Update is the metadata freshness control,
as recorded in design D3.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save`: the saved row gains display-detail columns and every reader of a saved manga's
  metadata reads them.
- `manga-detail-page`: a saved manga's detail is served from its row, `cached`-flagged, with the
  enrichment overlay intact; only refresh or a cold miss calls the plugin.
- `detail-enrichment-fields`: the persistence requirement narrows — candidates never write
  `saved_manga`, the resolver's materialized result does.

## Dependencies

None. The migration and the entity change MUST land in one commit (design D5): a build with the new
columns against an old database breaks every `saved_manga` read.

## Impact

- `src/store/` — entity columns, additive migration, `saved_detail_put`.
- `src/http/http_api_title.cpp` — read-first response, route-level materialization.
- `src/http/api_ctx.hpp` — full-refresh materialization, `cached` semantics.
- `src/http/http_api_enrich.cpp` — reset clears the materialized fields.
- `src/http/http_api_library.cpp` — placeholder-name guard on upsert.
- `src/http/http_pages.cpp` — library card name from the row.
- `templates/detail.*` — hydrate from the row-served response (no shape change expected).
- `tests/test_store.cpp`, `tests/test_http_enrich.cpp`, `tests/test_saved_stats.cpp`.

## Out of scope

- Storing chapters or pages (they stay cache-only).
- Backfilling detail for saved rows never opened: the first open after the migration resolves them.
- Cross-plugin detail sharing (a MangaDex enrichment result for a mangafreak row) — the row is keyed
  `(plugin, slug)` as today.
- Making search-result cards read `saved_manga` (design D11).
