---
type: decision
title: "Library page design (status, progress, done/new badges)"
description: "Design locked: title-level status/total/read/has_update, badges done/new, sidebar filter, mark_read+clear_update, placeholder covers; store+sync layer SHIPPED via library-grid-stats 2026-11-11 (16/16 tests), badge UI lands with library-grid-controls."
tags: [library, ui, schema]
generated: { by: agent/mcp, at: "2026-11-11T16:59:41Z" }
status: stable
---

Library page design, locked before any implementation (user directive: no coding until they green-light via staged openspec per page).

User decisions:
1. Granularity: title-level fields first, generically named (total/read), not per-chapter table. A chapter table is deferred until the per-chapter viewer exists.
2. Cover: placeholder (hue from id, as today) -- proxy /img + vips thumbnails is a later slice.
3. Interactions: easiest working path -- mark_read(id) sets read=total and clears the flag; POST route + htmx card swap.
4. Approach: 'important and functional first', no gold-plating.
5. Process: user will drive implementation themselves as staged openspec per page; agents prepare design only.

Schema additions to TitleEntity (sync_schema migrates): status TEXT default 'ongoing' (ongoing|hiatus|complete|canceled), total INT default 0 (chapters/pages unknown = 0), read INT default 0, has_update INT 0/1 (plugin sets on refresh; cleared on open/read).

Card badges:
- status chip: ongoing=blue, hiatus=amber, complete=green, canceled=gray
- done when read >= total && total > 0 (green)
- new when has_update=1 (red, takes priority over done)
- progress bar read/total under the title

Store additions: mark_read(id), clear_update(id); search() returns the new fields; sidebar filter All/Ongoing/Completed/New as query param on /library/grid.

Seed updated so every badge state is visible (fixed deterministic mix, not random at runtime).

# Related Concepts
- [Home demo: library grid, plugin filter, live log](home-demo.md): Library page design extends the home demo grid
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): Library page is a page of the core viewer architecture
