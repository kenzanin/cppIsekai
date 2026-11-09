# Proposal

## Why

The daemon has a search page and library grid, but no detail page: after finding a manga there is no place that shows its full metadata, chapter list, or an entry point into reading and downloading. The reference implementation (goIsekai `:8080/view/manga/...`) demonstrates the expected surface; cppIsekai already has every backend piece (plugin `detail`/`chapters` methods, stale-serve cache, `/export/cbz`, reader worker) but nothing wires them into a detail view.

## What Changes

- New page `GET /view/manga/<plugin>/<slug>`:
  - Cover (served via the existing `/cache/img` proxy) with title, tags, status, description from the plugin `detail` method (stale-serve with `cached:true` on failure)
  - Chapter list from the plugin `chapters` method (number, release date, per-row `⬇ cbz` button hitting the existing `/export/cbz` route)
  - `⟳ Update` button: re-pulls `detail` + `chapters` from the plugin, overwriting the stale-cache entries, then re-renders
  - Chapter rows link into the reader (existing window/worker path; no new reader behavior in this change)
- New JSON endpoints beside the page route for the page's Alpine fetches: `GET /api/detail/<plugin>/<slug>` and `GET /api/chapters/<plugin>/<slug>` (both stale-serve), plus `POST /api/refresh/<plugin>/<slug>` (forced re-pull)
- No cover management (candidates/pick/refetch), no enrichment editing, no library-toggle changes: deferred until a follow-up change

## Capabilities

### New Capabilities

- `manga-detail-page`: the detail view — metadata + chapter list rendering over the plugin data contract, stale-serve semantics, forced refresh, and the per-chapter cbz hand-off to the existing export route

### Modified Capabilities

<!-- none: existing specs (plugin-system, cache-layout, image-cache) keep their requirements; this change consumes them -->

## Impact

- `src/http/http_pages.cpp` (page route + template binding), `src/http/http_api.cpp` (3 JSON endpoints via the existing `call_cached` wrap), `templates/` (new `detail.mustache`)
- Reuses unchanged: plugin ABI (`detail`, `chapters`), `cache::ensure` stale layer, `/export/cbz`, `/cache/img`, reader window/worker
- No dependency changes; no store schema changes
