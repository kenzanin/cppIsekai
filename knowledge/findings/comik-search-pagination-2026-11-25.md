---
type: Finding
title: "comik search ignored ?page -- every UI page returned the same 48 cards"
description: "User reported /search?plugin=comik&q=return&page=3 identical to page 1. Plugin's methods.search fetched /search?word=<q> without the site's ?page=N and reported total=#items (48), so the host pager computed one page and appending page=3 still showed page 1. Site SSR honours ?page=N with 48 cards/page and exposes no total in Qwik state. Fix: forward params.page, and memoize a per-query last-page probe (exponential + binary) to report real pages/total (16 pages / 729 for 'return'). Verified pages 1/2/3/16/17 distinct, page 17 empty, real browser shows different first cards, full suite 22/22."
tags: [plugin, comik, pagination, search, fixed]
generated: { by: agent/mcp, at: "2026-11-24T22:47:27Z" }
status: stable
---

# Related Concepts
- Related to [Lua plugin end-to-end audit (search/detail/chapters/pages)](lua-plugin-audit-2026-11-16.md)
