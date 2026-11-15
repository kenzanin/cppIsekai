---
type: Constraint
title: "Plugin chapter numbers are untrusted: order, duplicates and placeholder 0"
description: "Plugin chapter lists arrive in arbitrary order and may carry duplicate or placeholder numbers (0) or non-numeric upstream numbers; /api/pages resolves prev/next by nearest value, and each plugin's parser needs a name-based fallback."
tags: [plugins, chapters, pages-api, prev-next, mangafire, mangafox]
generated: { by: agent/mcp, at: "2026-11-15T13:46:39Z" }
status: stable
---

Survey of the six plugins in the live library (2026-11-15):

| plugin | list order | duplicate numbers | note |
|---|---|---|---|
| mangafreak | DESC (36→1) | 5 (24b/24a, 9b/9a, ...) | source emits newest-first |
| atsumaru | ASC | 0 | decimals (9.5) fine |
| mangatown | ASC | 0 | decimals 6.1/6.2/7.1 fine |
| mangadex | ASC | 16 | real ch 0 exists (see decisions/chapter-zero-falsy-regression) |
| mangafox | ASC (cached) | 0 | guest emits DESC; reader pages broken, see below |
| mangafire | ASC | 1 (48 twice upstream) | entry with upstream number=0 |

## Rules this established

1. Never trust plugin list order. `build_pages_response` in `src/http/pages_api.cpp` picks prev/next by nearest numeric value (greatest below / smallest above), fixed in f8bcbc9 with a descending+duplicate test; `reader::next_window` also stable_sorts ascending. Verified live on mangadex, atsumaru, mangatown, mangafreak.
2. Duplicate numbers collapse to one navigation point — 24a and 24b cannot both be reached from the reader buttons. Reaching both needs index-based navigation, which would change the `/api/pages` contract; deferred.
3. A plugin parser must not pass an upstream placeholder through as 0. mangafire sends `number: 0` with `name: "48"` for a chapter that really is 48, which showed as a phantom "Chapter 0" holding 15 real pages (fixed 1e24d06: fall back to the leading number in the name when number is 0/nil).
4. mangafox `page`/reader is broken independently of numbering: `plugins_dev/wasm/mangafox/main.cpp` builds `/manga/<slug>/<ch>/1.html` but the site needs `/c<ch>/1.html` (c001 → 200, 001 → 404), and the mobile reader page now returns a JS shell with no `chapterid`/`imagecount` for the guest's regexes. Not fixed.

## Verification recipe

`curl -s localhost:8090/api/chapters/<plugin>/<slug>` and check the number sequence for order/dups/zeros, then `/api/pages/<plugin>/<slug>/<n>` for prev/next and page count at the boundaries. `/api/chapters` serves the api_cache row, so bust it (delete the `api_cache` row for that plugin) before believing a parser change — the plugin route with an extra `v=` param is the uncached path.

# Related Concepts
- [reader: chapter 0 is valid — falsy-zero regression fixed + unit-locked](../decisions/chapter-zero-falsy-regression.md): ch 0 is valid on mangadex; a plugin placeholder 0 must be told apart from a real prequel
