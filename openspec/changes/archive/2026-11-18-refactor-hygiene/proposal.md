# Proposal: refactor-hygiene

Refactor and test-hygiene work. No requirement changes, no user-visible behaviour change —
this change carries tasks only and has no spec deltas.

## Why

Four items from `references/rec-architecture-improvements`, each verified before being
proposed rather than taken on trust:

- **A1.** `src/store/store_search.cpp` is 1060 lines (re-measured 2026-11-18; was 794 at
  proposal time — chapter-state grew it). The convention says split units stay near 200
  code-loc. The file now mixes SIX concerns: the dedup/similarity core (fts, tokenize, dice,
  `recompute_dup_groups`), the plugin registry, search/count, `state_*`/`position_*`
  accessors, the `saved_*` writer family, and the chapter flags/pages family.
- **A2.** `src/http/enrich_internal.hpp` is 671 lines / ~20 KB of `inline` logic included by
  five TUs plus `api_ctx.hpp` — the same scale problem on the HTTP side.
- **A4.** `templates/detail_script.mustache` is 318 lines and
  `templates/reader_script.mustache` is 293 lines (measured). Alpine logic of that size lives
  in mustache partials, so it cannot be linted or unit-tested and it churns the template
  goldens on every edit.
- **A5.** The golden escape hatch is too quiet. Verified by probe: appending a comment to
  `templates/grid_body.mustache` does fail `test_templates` (2 assertions), so stale
  detection itself works. The real gap is that `CPPISEKAI_UPDATE_GOLDEN=1` re-locks the
  fixture in the same command that runs the check, so a stale suite becomes green without
  anyone reading the diff.
- **A6.** No precompiled header anywhere (`xmake.lua` has none): every TU re-parses the same
  frozen third-party forest (crow, glaze, spdlog, ...) pulled in via `api_ctx.hpp` /
  `plugin.hpp`. Clean build measured 26.5 s (2026-11-18). A PCH over the stable external
  includes is pure compile-time; it changes no behaviour and no diagnostics.

## What Changes

- **A1** split `store_search.cpp` by concern (dedup core / saved writers / chapter family /
  registry+search+state residue), keeping `compose_flags` as the single flag writer in one
  file.
- **A2** split the enrich unit the same way.
- **A6** add a precompiled header for the daemon target over frozen third-party includes
  only, with measured before/after build times; revert if the win is not measurable.
- **A4** extract the inline Alpine bodies from the two template partials into `static/*.js`
  served through the existing precompressed-asset route.
- **A5** make golden re-locking a deliberate two-step and add a stale check that fails.

## Not proposed, and why

- **A3 (serve Tailwind/Alpine/htmx as `.br`) is already done.** `static/tailwind.min.js.br`,
  `static/alpine.min.js.br` and `static/htmx.min.js.br` exist, `br_asset()` in
  `src/http/http_pages.cpp` serves them, and `templates/top.mustache` is down to 76 lines.
  Nothing to do; the recommendation is simply stale.
- **A6 (free-worker routing) is closed as documentation.** The open question — whether the
  model field is reachable — is answered and recorded in `findings/free-worker-routing`. No
  code change follows from it.
- **A7 (CBZ chapter order) was confirmed to be a real bug,** so it is not hygiene: it ships
  separately as `cbz-chapter-order` with a spec delta.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
_(none — pure refactor and test hygiene)_

## Impact

- `src/store/` — new split units, no behaviour change; `compose_flags` stays single-writer.
- `src/http/` — enrich split.
- `templates/` — two partials shrink to script tags; `static/` gains two assets.
- `tests/fixtures/golden/` — one deliberate re-lock for A4, reviewed.
- No API change, no schema change, no cache migration.

## Risk

The refactors are behaviour-preserving by intent, so the safety net is the existing suite:
`xmake test` must be green before and after each split, and the goldens must diff to no more
than the script tag. Any split that needs a behaviour change to compile has left the
refactor boundary and must be reported rather than absorbed.
