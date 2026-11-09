# Design

## Context

See proposal.md for motivation. Constraints that shape the approach:

- Plugin ABI already answers `detail` and `chapters` as normalized JSON; `call_cached` (single wrap point) already provides stale-serve with `cached:true`.
- Cache layout v2 keys covers and pages by `<plugin>/<slug>`; `/export/cbz?plugin=&slug=` and `/cache/img?url=` exist; reader window/worker run behind `/reader/*` endpoints.
- The search page's UI pattern is Alpine.js over Crow mustache templates; the detail page should follow it, not introduce a stack.

## Goals / Non-Goals

**Goals:**
- One page route + three JSON endpoints, all thin compositions over existing services
- Stale-first rendering identical to search-page semantics
- Per-chapter cbz buttons reusing `/export/cbz` with chapter-scoped slugs the cache layout already produces (`<slug>/chNNN`)

**Non-Goals:**
- Cover management (candidates/pick/refetch/clear) — future change
- Enrichment editing, library-toggle behavior changes — untouched
- New reader behavior — link into the existing `/reader/*` endpoints only

## Decisions

- **D1 — Page renders server-side, data hydrates client-side.** The mustache page carries layout + cover only; metadata and chapters arrive via Alpine fetches to the new JSON endpoints. Why: matches the search-page pattern, keeps the HTML route trivial, and lets `⟳ Update` re-fetch without a full page reload. Alternative (fully server-rendered page) rejected: refresh would need redirect-after-POST plumbing for little gain.
- **D2 — Endpoints live beside the existing API, not in the page.** `GET /api/detail/<plugin>/<slug>`, `GET /api/chapters/<plugin>/<slug>` (both via `call_cached`, stale-first), `POST /api/refresh/<plugin>/<slug>` (bypasses stale, overwrites cache on success via the existing store path of the stale layer). Why: same shape the plugins page/search page already use; keeps `http_pages` free of plugin calls.
- **D3 — Refresh = call-through + cache overwrite, sequential detail→chapters.** On any failure the refresh endpoint returns the first error and leaves cache untouched (spec: cache stays intact). Chapters numbering/date normalization stays the plugin's job; the endpoint does not re-normalize.
- **D4 — Chapter rows carry `chNNN` ids from the chapters data.** The cbz button and the read link both build from the same chapter identifier the cache layout uses, so no mapping table is introduced. A plugin that returns unparsable chapter numbers gets rows without actions (render-level guard, not endpoint-level).
- **D5 — slug safety.** Slugs come from our own cache/slug generation (fs-safe, lowercase); the page route 404s on anything containing path separators or `..` before touching the plugin. ponytail: full revalidation lands with cover management, when slugs become user-visible inputs from more surfaces.

## Risks / Trade-offs

- [Chapter payloads on long titles are big (hundreds of rows)] → chapter list renders client-side from one JSON fetch; pagination deferred until a real title hurts
- [Refresh endpoint is a write path on a GET-ish surface] → POST-only, plugin-failure-safe by D3; no auth model exists anywhere yet, consistent with the rest of the API
- [Plugin `detail` shapes can drift] → endpoints pass envelopes through unchanged; the page degrades field-by-field (missing description hides the block) rather than failing the page

## Migration Plan

Additive: new template, three endpoints, one page route. Rollback = revert commit; no schema, config, or cache-format changes.

## Open Questions

None deferred: slug scheme, scope, and URL form were settled with the user before the change was captured.
