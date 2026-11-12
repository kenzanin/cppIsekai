---
type: decision
title: "Plugin lifecycle: disabled = cache-only reads; folder-removal GC purges every trace"
description: "Disabled plugin = cache-only reads via a single call_cached gate, quiet scheduler/manual/UI, and folder-removal GC that purges registry + child traces in one transaction per plugin (change plugin-lifecycle, commit 5a41afc)."
tags: [plugin, store, scheduler, cache, lifecycle, sqlite, openspec]
generated: { by: agent/mcp, at: "2026-11-12T15:57:28Z" }
status: stable
---

## User ruling (2026-11-12, change plugin-lifecycle, commit 5a41afc)

A disabled registry plugin has EXACTLY these effects:

1. gone from the search source picker (client-side `enabled` filter, pre-existing)
2. skipped by the auto-update scheduler
3. manual update dead (UI + API)
4. still listed in the Library and readable like offline
5. deleting its folder purges the registry row and every child trace

## Mechanics

- Single disabled-gate: `Store::plugin_enabled(plugin_id)`; an absent registry row counts as enabled (legacy/seed rows carry plugin_id='').
- Read path: the gate lives in `call_cached` (src/http/stale.hpp) — cache hit serves as usual, the network fallback is skipped, no-cache answers `{"ok":false,"error":"plugin disabled"}`. The former ok:false pre-gates in title_data_response / /api/pages were REMOVED so cached detail/chapters/pages keep serving (offline-style reading).
- Scheduler: `update_candidates` gates each saved title on `COALESCE((SELECT enabled FROM plugin p WHERE p.plugin_id = s.plugin),1)=1`.
- Manual: `refresh_title` returns kDisabled → `/api/refresh` answers ok:false `plugin disabled`; `/view/manga` injects `pe` (1/0) and the detail button is `:disabled="refreshing || !pe"`.
- Folder GC: `Store::plugin_prune_absent(keep_ids)` deletes title (via registry row id), plugin_state, reader_positions, saved_manga, title_meta, update_attempts, api_cache, then the plugin row — one transaction per removed id. `Host::load_all` prunes registry ids absent from the scan, short-circuited when the scan dir itself is missing (a mount glitch must not wipe the library; an emptied dir legitimately does); every removal is logged.

## Gotchas

- `plugin.name` is UNIQUE: test fixtures upserting registry rows must use names distinct from the seeded ones (`Asura Scans`, plugin_id stays the real key; seeds carry plugin_id='').
- Golden templates: the detail page now renders `pe`; regenerate via CPPISEKAI_UPDATE_GOLDEN=1 (tests/fixtures/golden/detail.mustache.html).
