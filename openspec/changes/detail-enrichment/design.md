# Design

## Context

See proposal.md for motivation. Constraints:

- The detail page hydrates client-side (Alpine) from `/api/detail` + `/api/chapters`; stale-serve wraps the plugin ABI (`call_cached`).
- MangaDex serves description (`attributes.description.en`), author/artist via `relationships[].type=author|artist` with `includes[]=author&includes[]=artist`, and recommendations via `GET /manga/<id>/recommendation` (returns `manga_recommendation` entries whose `relationships[]` point at the candidate manga; attributes carry `score`).
- Verified endpoints (web probe, 2026-11-10): `/manga/<id>/recommendation` 200 with score; no `/recommendation` collection, no `/recommendations`; candidate lookup via `/manga?ids[]=..&includes[]=cover_art` works.
- `/plugin/<id>/logo` already serves a plugin logo file (`plugin.cpp:logo_path`); templates reference it for attribution.

## Goals / Non-Goals

**Goals:**
- Detail page shows synopsis, author(s), artist(s), related reading, plugin name + logo
- Related works end-to-end against live MangaDex (recommendation endpoint + one batched ids[] lookup)
- Contract is additive: old payloads render unchanged

**Non-Goals:**
- Same-author fallback, recommendation-by-tag blending — explicit follow-up if wanted
- js/wasm plugin implementations — Lua only
- Cover management, editing enrichment, library toggle — covered by the `library-save` change
- Related entries get no CBZ export or reading position — plain `View` links

## Decisions

- **D1 — Plugin fields ride the existing detail envelope.** `canonical_manga` grows `description`, `authors: []`, `artists: []`; `detail` passes `includes[]=author&includes[]=artist`. Why: no new ABI method, no extra round trip, stale cache keys adapt automatically.
- **D2 — `related` is a separate plugin method, not inlined in detail.** Why: `detail` is the stale-served hot path; recommendations change independently (score drift) and need a different upstream call. Response reuses `canonical_manga` so the page renders the same card shape as search results. Score order is preserved from the API.
- **D3 — One batched candidate lookup.** `related` calls `/manga/<id>/recommendation`, then one `GET /manga?ids[]=a,b,c&includes[]=cover_art` for all candidates (limit: as returned, un capped by us; the page caps display). Why: avoids N+1 plugin calls; MangaDex supports repeated `ids[]`.
- **D4 — Related section is its own fetch** against `GET /api/related/<plugin>/<slug>` (stale-serve), so a recommendation failure never breaks the rest of the page. On failure the section hides silently.
- **D5 — Plugin name comes from `/api/plugins`-style server state, not hardcoded.** The page needs the human-readable plugin name; reuse the same name the plugins registry holds (already passed to templates via `layout()`/`home_context`) by injecting it into the detail page context instead of a client-side fetch.
- **D6 — Logo attribution uses the existing `/plugin/<id>/logo` route** with an Alpine `@error` guard that swaps in the plain name when the file is missing.

## Risks / Trade-offs

- [Recommendation endpoint shape changes or disappears upstream] → whole `related` method isolated; failure hides the section; detail keeps working
- [Payload growth on detail (authors/artists/description)] → one-time per stale refresh; no list rendering of all works, only names
- [Related 404s when recommendation list is empty] → render "No related entries" hint or hide; decided at apply time, either acceptable

## Migration Plan

Additive everywhere: new plugin fields (ignored by old clients), new template blocks (render only when present), one new API route, one new plugin method. Rollback = revert commit.
