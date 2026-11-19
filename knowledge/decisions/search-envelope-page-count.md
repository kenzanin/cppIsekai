---
type: decision
title: "Search envelope: plugin reports real total + optional data.pages; host pager prefers it"
description: "A plugin search envelope reports the site's real result total and may add data.pages when the site's page size differs from the host's PAGE=12; search_script.mustache prefers data.pages and falls back to ceil(total/PAGE)."
tags: [plugin-envelope, search, pagination, host-contract, batcave, ui]
generated: { by: agent/mcp, at: "2026-11-19T06:49:55Z" }
status: stable
---

# Search envelope page count

## Decision
The search envelope (`data`) carries two pagination facts and the host pager consumes both:

- `total` — the site's **real** result count, never `#items` of the current page.
- `pages` — optional, and only when the site's page size differs from the host's `PAGE = 12`.

`templates/search_script.mustache` prefers `data.pages` and otherwise keeps the
original `Math.ceil(total / PAGE)` derivation, so plugins that emit only `total`
(mangaread 12/page, mangabuddy 18/page in its own envelope) are unaffected.

## Why
`PAGE = 12` is the host's assumption; sites paginate at their own size (batcave.biz
text search: 10 per page; its ComicList genre browse: 20 per page). With the plugin
reporting `total = #items` and a 10-card page, `ceil(10/12) = 1`, so the UI pinned
itself to page 1 and showed 10 of 21 results — the reported undercount.

Either half alone is not enough: a correct `total` with a wrong per-page divisor
still caps reachable pages, and `data.pages` removes the divisor from the equation.

## Mechanics that matter (batcave)
- Page count must come from the site's own pager, never from the requested page:
  an out-of-range page still serves the last page's cards, so trusting `page`
  inflates the count (`q=a&page=11` claimed 11 pages for a 10-page result set).
  Read the pager block's link numbers **and** its bare `<span>N</span>` current page,
  take the max.
- The total needs the last page's item count; fetching it once per query and
  memoising (bounded, process-lifetime) keeps it to one extra upstream fetch
  instead of one per host page.
- After changing the envelope shape, purge that plugin's `api_cache` rows —
  `call_cached` is local-first and pre-fix rows shadow the corrected handler.

## Evidence (2026-11-19)
`q=sex` → 21 results / 3 pages (site: 21); `q=a` → 100 / 10 and an out-of-range
`page=11` clamps to 10; genre `Drama` → 940 / 47; zero hits → 0 / 1. Live browser
(CDP): "21 results", pager 1-2-3, clicking 3 shows the single last title.
Commit `b551893`.
