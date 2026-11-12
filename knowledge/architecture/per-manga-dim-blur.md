---
type: Decision
title: Per-manga dim+blur cover toggle
description: "Dim+blur is per-manga: toggled from detail page, stored as host/dim/<plugin>/<slug>=1 in plugin_state, applied client-side to cover imgs carrying plugin+slug (grid/detail; search exempt). User-verified 2026-11-12. Gotchas: crow::json::wvalue for dim-list, htmx:afterSwap repaint, async Alpine cover needs $nextTick repaint."
tags: [library, ui, display, cover, detail, live-verified]
generated: { by: agent/mcp, at: "2026-11-12T03:20:07Z" }
status: stable
---

Goal: hide/blur NSFW covers on a per-title basis from the detail page, not a global switch.

Implementation (2026-11-12, user-verified working "mantrap done"):
- DB key: `host/dim/<plugin>/<slug>` = "1" in `plugin_state` (host bucket, same pattern as host/skip/<id>).
- `Store::state_keys_value(plugin_id, key_prefix, value)` — LIKE-escaped prefix + value filter; backs GET `/api/display/dim-list` returning `[["plugin","slug"],...]` built with crow::json::wvalue.
- POST `/api/display/dim-blur` takes `{plugin, slug, on}`; 400 on missing/invalid plugin (rejects `/`) or slug.
- `templates/top.mustache`: `coverKey()` parses img src query (`cover=1` + `plugin` + `slug`); `dimSet` hydrated from `/api/display/dim-list` on DOMContentLoaded; `paintDim()` toggles class `img.cover-dim` (brightness .35, blur 6px) and `[data-dim-toggle]` button labels. `toggleDimBlur(plugin, slug)` toggles one pair; detail button passes literal plugin/slug via Alpine `@click`. `window.paintDim` is exposed for late renders.
- Search page imgs carry no `slug` param → coverKey returns null → never dimmed (user-approved).
- Golden fixtures regenerated; `ctest -R "templates|pages"` pass.

Gotchas (all hit in production this session):
1. dim-list JSON MUST be built with `crow::json::wvalue` — an earlier hand-rolled string concat (jesc lambda) got mangled by heredoc escaping and emitted literal C++ source into the JSON body.
2. htmx swaps (`/library/grid` into `#grid`) replace cover imgs without a page load — repaint via `window.addEventListener('htmx:afterSwap', paintDim)` in top.mustache (window, not document.body: head script runs before body exists).
3. Detail-page cover renders ASYNC (Alpine `x-if="detail.cover"` after fetch): paintDim at DOMContentLoaded misses it — button state correct, image undimmed. Fix: detail boot() calls `this.$nextTick(() => window.paintDim())` after `await this.load()`.
4. Mixed binary/template state looks like a broken feature: templates render from disk per request, endpoints come from the compiled binary. Restart the daemon after rebuilding before judging template bugs.
