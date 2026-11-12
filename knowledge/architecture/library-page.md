---
type: decision
title: "Library page design (status, progress, done/new badges)"
description: "Design locked: title-level status/total/read/has_update, badges done/new, sidebar filter, mark_read+clear_update, placeholder covers; store+sync layer SHIPPED via library-grid-stats 2026-11-11 (16/16 tests), badge UI lands with library-grid-controls."
tags: [library, ui, schema]
generated: { by: agent/cli, at: "2026-11-12T00:58:45Z" }
status: stable
---

Design locked: title-level status/total/read/has_update, badges done/new, sidebar filter, placeholder covers; store+sync layer SHIPPED via library-grid-stats 2026-11-11 (16/16 tests); badge UI landed via library-grid-controls 2026-11-12 (12/12 tasks, 16/16 ctest).

Badge semantics (user clarification 2026-11-12, CRITICAL):
- New (red) = has_update=1 (plugin reports chapter growth).
- Done (green) = ALL available chapters read (read_ch >= total_ch AND total > 0). NOT a plugin status.
- Complete (green-ish filter) = plugin-reported status 'Completed' (manga tamat). Status values vary in casing per plugin (atsumaru: 'Completed'/'Ongoing', mangadex: 'ongoing') — compare case-insensitively, normalize to 'completed'.
- Overlay layout 2026-11-12: New/Done badge + status chip on ONE line top-left (text-[12px], translucent bg: red-600/70, emerald-600/70, black/40); plugin button (name + logo /plugin/<id>/logo) bottom-left, plugin_id-keyed. Transparent bg via Tailwind opacity modifiers — no SVG assets needed.

Client grid (library-grid-controls D1): saved-mode grid fully client-side (x-data gridPage) — data-* attrs on cards, q/plugin/filters/sort/page state client-side, 12/page, pager + empty state, 'Reset filters' + sidebar 'N titles · reset filters' card both dispatch grid:reset CustomEvent; sidebar plugin rows dispatch grid:plugin (saved mode) with plugin_id (NOT display name — display name broke matching; commit ba9334d); active sidebar row toggled via data-grid-plugin syncSidebar(). ?plugin=<id> deep-link preselects filter on load. hx-get stays for non-saved mode deep-link nav.

Sidebar counts = saved_manga by plugin_id; row link in saved mode = client filter; logo via /plugin/<id>/logo; enrichment alts fed to saved_manga (see library-dedup). Commits: 8e4cc2b (overlay one-row + complete filter), 047d450 (logo overlay 2x font), ba9334d (plugin_id filter), a34960d (controls ship).
