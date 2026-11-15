---
type: decision
title: "UI polish: category filters, card dim toggle, accessible names, and dark contrast fix"
description: "Status category filter chips (ongoing/hiatus/canceled/completed) and per-entry dim toggle on library grid, accessible names (aria-label) on icon-only controls, and dark-theme contrast fix across templates (text-zinc-400)."
tags: [ui, templates, accessibility, contrast, grid, status-flags]
generated: { by: agent/mcp, at: "2026-11-15T00:11:47Z" }
status: stable
---

# UI polish: category filters, card dim toggle, accessible names, and dark contrast fix

Shipped via `openspec/changes/ui-polish` (commit `5a1c2b8`).

## 1. Grid Category Filter & Neutral Chip
- Added filter chips for stored `status_flags` category bits: Ongoing (bit2=4), Hiatus (bit3=8), Canceled (bit4=16), and Completed (bit5=32).
- Filter state operates under the unified tri-state `toggleBadge(kind)` (`off` -> `show` -> `hide` -> `off`).
- Cards whose status does not fold to any known category (category mask `status_flags & 60 == 0`) render a neutral chip with their raw `status` string.
- Shares one client-side pagination reset with New and Done filters.

## 2. Card Dim Toggle
- Dim/blur toggle added to each saved manga card on the library grid, mirroring the detail page.
- Persisted via `POST /api/display/dim-blur` directly updating `status_flags` bit6 (is_dim).
- Reflects the server state via `paintDim()` and `data-dim-toggle="'{{plugin}}/{{slug}}'"`. Unsaved cards do not expose the toggle.

## 3. Accessible Names & Focus Rings
- Added `aria-label` attributes to icon-only buttons across templates (`templates/grid_body.mustache`, `templates/detail.mustache`, `templates/reader_top.mustache`, `templates/reader_bottom.mustache`).
- Ensured focus indicators (`focus-visible:ring-2 focus-visible:ring-zinc-500`) remain visible on dark zinc surfaces.

## 4. Dark Theme Contrast Enhancement
- Low-contrast dark text classes (`text-zinc-600`, `text-zinc-700`) on dark background (`bg-zinc-950` / `bg-zinc-900`) bumped to `text-zinc-400` / `text-zinc-300` across `detail.mustache`, `plugins.mustache`, `reader_top.mustache`, and `search.mustache`.
- Chapter unhide/hide and read/unread icon buttons bumped from `text-zinc-600` to `text-zinc-400 hover:text-zinc-200`.

## 5. Verification & Locks
- Locked in `tests/test_templates.cpp` (`TEST_CASE("templates: ui-polish controls and aria-label presence")`).
- Golden fixtures re-locked via `CPPISEKAI_UPDATE_GOLDEN=1`.
- Full suite green: 18/18 test targets passed (100%).

# Related Concepts
- [Recommendation: UI polish backlog (for another model to pick up)](../references/rec-ui-polish.md): Implementation of U1, U3, U5, and dark-theme contrast fix from rec-ui-polish
- Related to [Per-manga dim+blur cover toggle](../architecture/per-manga-dim-blur.md)
- Related to [status_flags bitfield: bit0-6 layout + single compose_flags writer](../architecture/status-flags-bitfield.md)
