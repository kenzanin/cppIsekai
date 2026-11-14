---
type: reference
title: "Recommendation: UI polish backlog (for another model to pick up)"
description: "TRIAGED 2026-11-14 against the tree: U6 (settings provenance) was already shipped and is struck; U1, U3 and U5 are open and owned by openspec/changes/ui-polish; U7 is unblocked because its A3 dependency already shipped; U2 and U4 remain unstarted. Original: small independently shippable UI polish items, none browser-verified."
tags: [recommendation, ui, backlog, templates, model-harness]
generated: { by: agent/mcp, at: "2026-11-14T10:50:28Z" }
status: draft
---

Written 2026-11-14. UI stack: crow + mustache templates, Alpine for state, htmx, Tailwind play script. Pages: home, search, grid (library), detail, reader, plugins, logs, settings.

## Ground rules

- Every item touches templates, so it changes golden files. Re-lock goldens only after reading the diff (see `references/rec-architecture-improvements` A5).
- No code was run in a browser for this document. Items marked B need a browser look before and after; do not trust a build/test pass for visual work.
- Keep `status_flags` semantics: badges are driven by the bitfield (`architecture/status-flags-bitfield`); the UI must not recompute New/Done from other columns.

## Items

### U1. Library grid status badges and category filter (B)
> **→ `openspec/changes/ui-polish`** (triaged 2026-11-14). Partly shipped, partly not: the grid already has
> tri-state New/Done/Complete controls reading `status_flags` bits, but it has **no** control for the category
> bits (ongoing/hiatus/canceled/completed) even though the store now carries them. The neutral-chip path for
> an unrecognised status is unproven. Open.
Grid now has category bits (ongoing/hiatus/canceled/completed) and the Complete filter reads bit5. Check: badge colours distinct and readable, unknown-status titles (category mask 0) still render a neutral chip from the raw `status` string, filter counts match the visible cards.

### U2. Detail page chapter list (B)
`decisions/chapter-list-controls` documents the client pager and the Alpine `x-for` pitfalls (no `$index` in keys, exactly one root element). Polish candidates: keyboard navigation of the pager, visible read/skip state without hover, sticky chapter header on long lists. Must not violate the two pitfalls.

### U3. Accessibility pass (I)
> **→ `openspec/changes/ui-polish`** (triaged 2026-11-14). Measured instead of estimated: `aria-label` appears
> 10 times across `templates/`, in `reader_top.mustache`, `reader_bottom.mustache` and `detail.mustache`.
> `grid_body.mustache` has **none**, so its icon-only controls (badge filters, per-card plugin button) are
> unnamed. Open.
`templates/grid_body.mustache` has `alt="{{name}}"` and `loading="lazy"` on covers (V), but the per-card plugin button and icon-only buttons need aria-labels, and focus rings must stay visible on dark zinc backgrounds. Run through every template once and list gaps before fixing.

### U4. Reader polish (B)
Four reader modes and zoom (fit H / fit W / 1:1) exist (`architecture/reader-page-progress`). Candidates: remember last mode per manga, clearer loading/placeholder state while the 10-page window fills, error tile for a page whose fetch fails (stale-serve already exists server-side).

### U5. Dim/blur toggle discoverability (B)
> **→ `openspec/changes/ui-polish`** (triaged 2026-11-14). Confirmed one-sided: the toggle exists in
> `templates/detail.mustache` and **not** in `templates/grid_body.mustache`, so a dimmed entry can only be
> dimmed or undimmed from the detail page. Open.
Dim is `status_flags` bit6 (`architecture/per-manga-dim-blur`). Check the toggle is reachable from both grid and detail and the dimmed state is obvious without looking at the cover.

### ~~U6. Settings page (I)~~
> **DONE — dropped from the backlog** (triaged 2026-11-14). Already shipped: the `settings-page` spec
> requirement is literally titled "Settings page lists every registry knob with provenance", and
> `src/config/config.hpp` returns the winning layer as `source` per knob. The remaining candidate from this
> item (inline validation errors from `POST /config` merge-save) was never claimed as shipped and is not
> tracked — reopen it as its own item if it is wanted.
`architecture/settings-page-openspec`: GET /config projects the registry with layer provenance. Candidates: show provenance (default/ini/env/flag) next to each value, mark env/flag-locked knobs read-only, inline validation errors from POST /config merge-save.

### U7. Page weight and load (V: Tailwind play script 102 KB br, Alpine 19 KB, htmx 13 KB)
> **Unblocked** (triaged 2026-11-14). The A3 dependency is already satisfied — the assets ship as
> `static/*.min.js.br` and are served by `br_asset()`, so the "play script" in this heading is stale: the page
> loads the precompressed build, not the in-browser JIT. Re-measure before believing any target is still
> worth chasing; `refactor-hygiene` A4 moves two more inline scripts into that same route.
Depends on A3 in the architecture backlog; do A3 first, then re-measure.

## Acceptance for any item

1. `xmake -y` zero warnings, `xmake test` green.
2. Golden diff reviewed, limited to the intended markup.
3. For B items: a short browser smoke note (what was clicked, what was seen) added to the change's `tasks.md`.

# Related Concepts
- [Recommendation: architecture improvements backlog (for another model to pick up)](rec-architecture-improvements.md): Architecture backlog; A3 and A5 gate parts of the UI work.
