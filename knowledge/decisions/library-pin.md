---
type: decision
title: "Pinned titles: a flag on saved_manga, rendered as its own grid section"
description: "Library pinning (2026-11-20): pinned is a boolean column on the saved_manga row (entity + upsert + saved get/set + /api/library payload), and /library renders two grids — pinned above the existing library — instead of a second table or a parallel list."
tags: [library, pin, grid, store, saved-manga, ui]
generated: { by: agent/mcp, at: "2026-11-20T13:01:21Z" }
status: stable
---

## Decision

A pinned title is a property of the library entry it already has:

- `saved_manga.pinned` boolean, added to SavedMangaEntity + mapper (src/store/store_internal.hpp), the schema/upsert in store_core/store_saved, the saved get/set projection in store_api.cpp, and the /api/library payload (http_api_library.cpp).
- The detail action row gets a pin toggle (templates/detail.mustache + static/detail.js): "☆ Pin" ⇄ "★ Pinned", POSTing the pin route.
- /library renders two grid sections from one payload: the pinned grid first, then the unchanged library grid (grid_body.mustache + the page builder in http_pages.cpp). No second listing endpoint, no duplicated card markup.

## Why

Pinning is presentation of an existing library row, not a new collection: a separate pinned table would need its own upsert, delete and enrichment rules, and would let a pin outlive (or precede) the library entry it points at. A flag keeps one source of truth and makes "pinned is also in the library" impossible to violate.

## Traps paid for

- After changing templates/*.mustache, tests/test_templates.cpp goldens must be re-locked (detail/grid/home all shifted) and the diff reviewed — loosening assertions hides real changes.
- static/detail.js is brotli-served: regenerate static/detail.js.br or the toggle ships dead.

## Verified 2026-11-20

Pinned from the real detail button, /api/library reported the flag, /library rendered the Pinned section with the card and the Library grid unchanged below; unpinning emptied the section and the wrapper went display:none. Suite 19/19. Commit 37ad588.

# Related Concepts
- Related to [Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)](saved-manga-detail-source-of-truth.md)
