# Proposal

## Why

The manga detail page shows only `{name, cover, status, tags, release_at}` from the plugin `detail` method. Readers need the synopsis, authors/artists, and "related reading" (MangaDex `?tab=recommendations`) to decide what to read next, plus visible plugin attribution (name + logo). MangaDex already serves all of this; the plugin simply drops it.

## What Changes

- Plugin (`plugins_dev/lua/mangadex/main.lua`):
  - `canonical_manga` grows `description`, `authors`, `artists` — `detail` calls with `includes[]=author&includes[]=artist` and maps `attributes.description.en` plus author/artist relationship `attributes.name`.
  - New method `related`: `GET /manga/<id>/recommendation` (returns `manga_recommendation` objects with a `score`; the other type relationship is the candidate), then one batched `GET /manga?ids[]=...&includes[]=cover_art` to materialize candidates into the canonical-manga shape, sorted by score desc. No same-author fallback.
- Detail page (`templates/detail.mustache`): renders synopsis (`description`), author(s), artist(s), provider footer (plugin name + `/plugin/<id>/logo` logo, linking nowhere), and a Related section (cover thumbnails + titles, `View` links to the detail route; capped at what the API returns).
- JSON: `GET /api/related/<plugin>/<slug>` (stale-serve, same pattern as `/api/pages`) backing the Related section.
- Stale layer unchanged (detail envelope cache key gains the new fields automatically).

## Capabilities

### New Capabilities

- `detail-enrichment`: the enriched detail contract (synopsis, authors, artists, related reading, plugin attribution) over the plugin data contract

### Modified Capabilities

<!-- none: manga-detail-page keeps its requirements; this change adds fields and a section -->

## Impact

- `plugins_dev/lua/mangadex/main.lua` (canonical_manga + new `related` method)
- `src/http/http_api.cpp` (or `http_pages.cpp`) for `/api/related/...`; `/api/detail` response shape passes through new fields unchanged
- `templates/detail.mustache` (synopsis/author/artist/related/footer blocks)
- Plugin contract is additive: search/pages/chapters payloads untouched; js/wasm runtimes untouched (Lua only in this change)
- No store schema changes; no dependency changes
