---
type: Decision
title: Per-manga dim+blur cover toggle
description: Per-manga dim+blur is stored as status_flags bit6 (is_dim) on the saved_manga row; applied client-side to cover imgs carrying plugin+slug (grid/detail; search exempt); unsaved manga cannot be dimmed.
tags: [library, ui, display, cover, detail, live-verified]
generated: { by: agent/mcp, at: "2026-11-14T10:27:48Z" }
status: stable
---

Goal: hide/blur NSFW covers on a per-title basis from the detail page, not a global switch.

Current storage (status-flags-followup-cleanup, 2026-11-14; supersedes the 2026-11-12 plugin_state key):
- The dim state is `status_flags` bit6 (value 64) on the `saved_manga` row. `plugin_state` key `host/dim/<plugin>/<slug>` is retired.
- `Store::saved_dim_update(plugin, slug, on)` sets or clears bit6 only; a no-op when the manga is not saved (no row to hold the bit).
- `Store::saved_dim_list()` returns the `(plugin, slug)` pairs with bit6 set; backs GET `/api/display/dim-list` as `[["plugin","slug"],...]` built with `crow::json::wvalue`.
- POST `/api/display/dim-blur` takes `{plugin, slug, on}` and calls `saved_dim_update`. Unsaved manga: request succeeds, nothing persisted.
- All stat writers preserve bit6 via the single `compose_flags` helper (see architecture/status-flags-bitfield).

Client (`templates/top.mustache`): `coverKey()` parses the img src query (`cover=1` + `plugin` + `slug`); `dimSet` hydrates from dim-list on DOMContentLoaded; `paintDim()` toggles class `img.cover-dim` and `[data-dim-toggle]` labels. `window.paintDim` is exposed for late renders. Search page imgs carry no slug, so they are never dimmed.

Gotchas:
1. dim-list JSON MUST be built with `crow::json::wvalue`; a hand-rolled string concat once emitted literal C++ source into the body.
2. htmx swaps replace cover imgs without a page load; repaint via `window.addEventListener('htmx:afterSwap', paintDim)` in top.mustache.
3. The detail-page cover renders async (Alpine after fetch): call `this.$nextTick(() => window.paintDim())` after load.
4. Templates render from disk per request but endpoints come from the compiled binary; restart the daemon after rebuilding before judging template bugs.
