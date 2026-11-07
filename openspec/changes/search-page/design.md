# Design: search-page

## Context

Plugins are called through the existing `/plugin/<id>/<method>?<query>` route; the page is
pure JS against it. The plugins-page ruling (disabled = unsearchable) applies to the picker.

## Goals / Non-Goals

- Goals: one-plugin search with genre filter, pagination, honest empty/error states, deep
  linkable state.
- Non-Goals: detail navigation from results (separate `mangadex-detail` change), multi
  plugin fan-out, search history, server-side rendering of results.

## Decisions

**1. ABI search shape: `{items,total}` + `page`; `data` array is retired.**
The only consumer is our UI; changing now while one plugin exists is the cheap moment.
`total` enables real pagination; `page` is 1-based, plugin clamps to range.

**2. Genres are a capability probe, not a contract.**
Page calls `genres` after picking a plugin; network/`ok:false`/missing method -> row hidden.
Mangadex implements it from `/manga/tag` (name.en list, cached in memory for the session —
the tag list is static-ish; refresh per page load is fine at v1, no cache).

**3. Tag filter is AND, passed as plugin-opaque strings.**
UI sends `tags=Isekai,Fantasy` (URL-encoded, comma-joined display names); mangadex maps
names -> uuid `includedTags[]`. Plugin-owned mapping keeps the ABI site-agnostic.

**4. State lives in the URL; JS is a renderer.**
`?plugin=&q=&tags=&page=` — load reads it, every action pushes it (`history.pushState`).
Refresh/back/share work with zero server templates per state.

**5. Pagination mirrors the library grid idiom.**
`Prev/Next` + `page/pages` from `total` and page size 12 (plugins-page count aesthetic);
buttons render above and below the grid, disabled at bounds. Page size fixed, not a setting
(YAGNI until someone asks).

**6. Errors are inline, never modal.**
Picker plugin missing/disabled -> notice + picker focus; call `ok:false` -> error line above
grid with retry button. Loading = skeleton shimmer on the grid (CSS only).

## Risks / Trade-offs

- [Breaking ABI shape] single consumer, changed in same commit
- [genres roundtrip per load] one extra call; cache later if it shows
- [tag name -> id mapping in plugin] MD-specific but lives in the plugin where it belongs

## Migration Plan

1. mangadex: search rewrite (page/total/tags) + genres.
2. search.mustache + route.
3. Live verify: pick mangadex, query, filter, paginate, empty + error paths, URL reload.
