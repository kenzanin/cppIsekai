---
type: Fact
title: "Rec backlog triage: 2 items already done, A7 proven bug, 3 OpenSpec changes opened"
description: "Triage of the two recommendation backlogs against the tree: A3 (precompressed assets) and U6 (settings provenance) already shipped, A6 closed, A7 proven to be a real silent bug (ch1000 exported ahead of ch999, offsets 67 vs 154), A5 half-true (detection works, escape hatch hides it); three OpenSpec changes opened - cbz-chapter-order, ui-polish, refactor-hygiene."
tags: [backlog, triage, openspec, cbz, ui, refactor, verified]
generated: { by: agent/cli, at: "2026-11-14T11:17:52Z" }
status: stable
---

Triage of `references/rec-architecture-improvements` + `references/rec-ui-polish` on 2026-11-14. Every claimed item was checked against the tree instead of being taken on trust; two were already done and one was proven to be a real bug.

## Verified still open

- **A1** `src/store/store_search.cpp` is 794 lines; mixes FTS search, `saved_*` stat writers, `compose_flags`, dim, listing.
- **A2** `src/http/enrich_internal.hpp` 20.3K alongside `http_api_enrich.cpp`.
- **A4** `templates/detail_script.mustache` 318 lines, `templates/reader_script.mustache` 293 lines.
- **U1** grid filter is `{new, done, complete}` only; the category bits have no grid control.
- **U5** dim toggle exists in `templates/detail.mustache` but not in `templates/grid_body.mustache`.
- **U3** `aria-label` appears 10 times across `reader_top`, `reader_bottom`, `detail` — `grid_body.mustache` has none, so its icon-only controls are unnamed.

## Already done - dropped from the backlog

- **A3** (serve Tailwind/Alpine/htmx as `.br`) ships: `static/{tailwind,alpine,htmx}.min.js.br` exist, `br_asset()` serves them from `src/http/http_pages.cpp`, and `templates/top.mustache` is down to 76 lines.
- **U6** (settings provenance) ships: the `settings-page` spec already requires the winning layer per knob and `src/config/config.hpp` returns `source`.

## A7 is a real bug, proven not inferred

`cbz::build` accumulates entries in `std::map<std::string, fs::path>`, so archive order is lexicographic on `chNNN/pNNN.ext`. `cache_internal.hpp` pads chapter numbers to a *minimum* of three digits, so `ch1000` is wider and sorts before `ch999`.

A probe test (cached `ch999/p001.webp` + `ch1000/p001.webp`, exported, read the local-file-header offsets in the returned zip) gave `ch1000` at offset 67 and `ch999` at 154. Silent failure: the zip is valid, only the reading order is wrong. The main `cache-layout` spec licenses it by asserting that zero padding makes lexicographic order correct. The same requirement also promises a cover entry the code never writes (v3 cover files are content-hash named, no slug mapping).

## A5 is half-true

Stale detection works: appending a comment to `templates/grid_body.mustache` does fail `test_templates` (2 failed assertions). The gap is only the escape hatch - `CPPISEKAI_UPDATE_GOLDEN=1` rewrites the fixture and passes in the same invocation.

## A6 closed

Answered by `findings/free-worker-routing`; no code follows from it.

## Changes opened

- `cbz-chapter-order` - A7, with a `cache-layout` delta (numeric ordering, no cover entry).
- `ui-polish` - U1, U3, U5, declared as ADDED requirements so they do not collide with the unarchived `status-flags-followup-cleanup` delta. U6 excluded as shipped.
- `refactor-hygiene` - A1, A2, A4, A5 with `skip_specs: true` (behaviour-preserving, no delta).

All three pass `openspec change validate --strict`.

## Standing blocker

`status-flags-followup-cleanup` is complete (28/28) but **not archived**, so `openspec/specs/library-save/spec.md` still lists the dropped `has_update` column. Archive it before archiving `ui-polish`.

# Related Concepts
- [Recommendation: architecture improvements backlog (for another model to pick up)](../references/rec-architecture-improvements.md): triage of the backlog written in that reference
