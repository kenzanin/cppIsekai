---
type: reference
title: "Recommendation: architecture improvements backlog (for another model to pick up)"
description: "TRIAGED 2026-11-14 against the tree: A3 (precompressed assets) was already shipped and A6 is closed, both struck; A7 was proven to be a real silent bug and moved to openspec/changes/cbz-chapter-order; A1/A2/A4/A5 are open and owned by openspec/changes/refactor-hygiene. Original: prioritised self-contained architecture work items written after status-flags-followup-cleanup merged."
tags: [recommendation, architecture, backlog, refactor, model-harness]
generated: { by: agent/mcp, at: "2026-11-14T10:50:28Z" }
status: draft
---

Written 2026-11-14 after `status-flags-followup-cleanup` merged to main (9c55d4a). Not yet an OpenSpec change: pick an item, run `okf_search`, open a change named after it.

## Context and fixed decisions (do not relitigate)

- The project is a personal test harness for evaluating coding models. Work done by a small model stalled for ~2 hours on the status-flags change; a larger model finished it with a clean build. Treat items below as good benchmark-sized tasks.
- Packing booleans into `saved_manga.status_flags` (bitfield) is the OWNER'S choice. Do not propose reverting to separate bool columns. Rule already recorded in `architecture/status-flags-bitfield`: a bit earns its place only if boolean AND not derivable from the same row.
- Conventions come from AGENTS.md: -Wall -Wextra clean, one test target per module, English-only artifacts, Mermaid-only diagrams, branch per change, `--no-ff` merge.

## Verification level

V = verified by reading/measuring in this session. I = inferred from names/sizes, confirm before acting.

## Items (highest value first)

### A1. Split `src/store/store_search.cpp` (V: 794 lines)
> **→ `openspec/changes/refactor-hygiene`** (triaged 2026-11-14). Re-measured: still 794 lines. Open.
Convention says split units stay <= ~200 code-loc. This file mixes search, saved_* stats writers, `compose_flags`, dim, and listing. Split by concern (search / saved stats / library listing), keep `compose_flags` as the single flag writer in one file. Accept: build 0 warnings, `xmake test` all green, no behaviour change, `StatusFlag` writers still all in one translation unit.

### A2. Review `src/http/enrich_internal.hpp` (V: 534 lines, header)
> **→ `openspec/changes/refactor-hygiene`** (triaged 2026-11-14). Confirmed: 534 lines, 20.3K. Open.
Largest header in the tree; header-only is the stated preference but 534 lines recompiles into every includer. Check include fan-out first (use codebase-memory `trace_path`/`search_graph`), then decide split vs leave. Accept: measured incremental rebuild time before/after.

### ~~A3. Replace in-browser Tailwind (V: `templates/top.mustache` loads `/static/tailwind.min.js`, 102 KB brotli)~~
> **DONE — dropped from the backlog** (triaged 2026-11-14). Already shipped before this document was written:
> `static/tailwind.min.js.br`, `static/alpine.min.js.br` and `static/htmx.min.js.br` all exist, `br_asset()`
> in `src/http/http_pages.cpp` serves them with a plain fallback, and `templates/top.mustache` is down to
> 76 lines. Nothing left to do.
The Tailwind play script compiles CSS at runtime in every page load. Option: precompile one static CSS with the standalone Tailwind CLI at build time and serve it through the existing precompressed-asset route (`decisions/static-assets-br`). Needs a decision first: it adds a build step. Accept: no visible diff on golden render tests beyond the script/link tag, page weight drop recorded.

### A4. Move inline template scripts to static assets (V: `detail_script.mustache` 318 lines, `reader_script.mustache` 293 lines)
> **→ `openspec/changes/refactor-hygiene`** (triaged 2026-11-14). Line counts re-confirmed. Open.
Large Alpine logic lives in mustache partials, so it cannot be linted or unit-tested and shares the template golden-test churn (golden files were re-locked this session). Extract to `static/*.js` served as .br. Accept: golden diffs limited to the script tag; reader and detail behave the same in a browser smoke (see `decisions/browser-smoke-tooling`).

### A5. Golden-test hygiene (V: goldens failed before being re-locked in the last change)
> **→ `openspec/changes/refactor-hygiene`** (triaged 2026-11-14). Probed, and the claim is only half true:
> stale detection **does** work — appending a comment to `templates/grid_body.mustache` fails `test_templates`
> on 2 assertions. The real gap is narrow: `CPPISEKAI_UPDATE_GOLDEN=1` rewrites the fixture and passes in the
> same invocation, so a red suite turns green without anyone reading the diff. Fix the escape hatch, not the
> detector.
`CPPISEKAI_UPDATE_GOLDEN=1` made a red suite green without anyone reviewing the diff. Add a CI-style check: fail `xmake test` when goldens are stale, and make re-lock a deliberate two-step (update, then review `git diff tests/fixtures/golden`). Accept: a deliberate template edit without re-lock fails the test.

### ~~A6. Free-worker routing follow-up (I: see `findings/free-worker-routing`)~~
> **CLOSED** (triaged 2026-11-14). The open question — whether a specific model is reachable — is answered
> in `findings/free-worker-routing` and needed no code change: `spawn_agents` has no model field, and a free
> model is reachable both through a native `runAs: subagent` skill with `model: 9router/free` and through the
> opencode CLI (`run -m 9router/free`). Nothing follows from it.
That finding was written during the last change; read it, decide whether it is still an open gap, and either close it or turn it into a change.

### ~~A7. Known cache backlog still open (V: recorded, not fixed)~~
> **MOVED → `openspec/changes/cbz-chapter-order`** (triaged 2026-11-14). Confirmed to be a **real bug**, and
> worse than "recorded, not fixed": it fails silently. `cbz::build` accumulates entries in a
> `std::map<std::string, fs::path>`, so archive order is lexicographic on `chNNN/pNNN.ext`, while
> `cache_internal.hpp` pads chapter numbers to a *minimum* of three digits — so `ch1000` is wider and sorts
> **before** `ch999`. A probe test (cached `ch999/p001.webp` and `ch1000/p001.webp`, exported, read the
> local-file-header offsets in the returned zip) gave `ch1000` at offset 67 and `ch999` at 154: chapter 1000
> opens first. The zip stays valid, only the reading order is wrong. The main `cache-layout` spec also
> licenses it by claiming zero padding makes lexicographic order correct, and promises a cover entry the code
> never writes. Its own change now, not this backlog.
`references/cache-v3-followup-backlog`: CBZ builder sorts chapters lexicographically (ch999 > ch1000). Small, self-contained, good first task.

## Out of scope

- Reverting the bitfield, new bits for derivable columns, or moving `reader_positions`/`update_attempts` data into `saved_manga` (all rejected in `architecture/status-flags-bitfield`).

# Related Concepts
- [Recommendation: UI polish backlog (for another model to pick up)](rec-ui-polish.md): UI items U7 depends on architecture item A3.
