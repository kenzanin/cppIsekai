---
type: Finding
title: Client-side class painting after a fetch = FOUC; SSR the class instead
description: "Grid dim+blur covers flashed unstyled on every refresh: paintDim ran after the /api/display/dim-list fetch (DOMContentLoaded), so the first paint was always unstyled - asset load order was NOT the cause. Fix: SSR the class (grid rows carry status_flags bit6 -> cover-dim in the mustache); client repaint stays for toggle/htmx/detail-async only."
tags: [frontend, ssr, fouc, dim, grid, fixed]
generated: { by: agent/mcp, at: "2026-11-23T07:41:05Z" }
status: stable
---

## Symptom

Dimmed covers flashed un-dimmed for a moment on every page refresh (spoilers visible).

## Root cause

The dim class was applied client-side: `DOMContentLoaded -> fetch /api/display/dim-list ->
paintDim()`. The first paint could never be styled - the data arrived after render. Reordering
tailwind/alpine/htmx or embedding assets would not have changed this.

## Rule

When a server-known per-row state styles content, render the class server-side; keep client JS for
mutations only (`paintDim` stays for the toggle, `htmx:afterSwap` and the async detail cover).

## Fix

- `http_pages.cc` saved-row ctx: `dimmed = status_flags & kDim`
- `grid_body.mustache`: `class="...{{#dimmed}} cover-dim{{/dimmed}}"`

Commit 3d4d094. Related: decisions/library-promote-on-save (same batch, unsave wipe + SSR notes).

# Related Concepts
- [Library promotion: api_cache is the staging store; chapters become a read source (planned, openspec library-promote-on-save)](../decisions/library-promote-on-save.md): Same batch: the grid wipe/SSR work surfaced the FOUC; dim class now server-rendered from status_flags.
