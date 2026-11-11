---
type: Fact
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Detail page + enrichment (shipped 2026-11-11, 26/26 tasks): recommendations kind (MangaDex/MangaUpdates, cap 5) merged into Related; fetched keyed (source,kind); chosen cover overlays library grid + /api/library; covers cap 5; tags attributes.tags; MU Title-Case; combined alt-titles picker (unicode-aware dedupe); layout restructured (unclosed header/meta divs fixed 607ea9e; golden regen 2d91dcf); golden-file render lock (decisions/template-golden-test). Attribution row under title = ONE flex line (merged 2026-11-11): plugin logo + pluginName + status + release date + cached badge; logo guarded x-if=\"pluginName && pluginLogoOk\"; container x-show = pluginName || detail.status || detail.release_at || cached. Chapter pager 20/page client-side, single-root x-for button — see decisions/chapter-list-controls. Daemon :8090."
tags: [detail, enrichment, cover, wizard, cache, plugin]
generated: { by: agent/mcp, at: "2026-11-11T15:18:43Z" }
status: stable
---

## Enrichment architecture facts (2026-11-11, shipped + applied)
- Enrichment = dedicated plugins, **separate path**: `./data/enrichment/<id>/*` (NOT `./data/plugins`). Same machinery (Lua/sol2, http.fetch, storage.*, log.*, ApplyProxy); only directory capability (`enrichment`) differ. Sources: MangaDex + MangaUpdates.
- Two fetch paths: fetch detail (title, synopsis, artist, author, tags) vs fetch covers (heavier). Trigger: explicit user buttons on saved manga only. Never implicit.
- **Original** `saved_manga` never overwritten → Reset reverts source free.
- **title_meta** single row per (plugin, slug): `fetched` JSON array, entries keyed (source, kind), `kind` ∈ detail|covers|recommendations; missing = legacy detail entry served for `detail` only. `chosen` user overrides. Display = `chosen.<field> ?? source cache`. Reset detail clears `chosen`.
- Combined alt-titles picker: MangaDex alTitles + MangaUpdates associated merged, normalized, unicode-aware dedupe; single picker UI (de0009f, b6a35ea). Chosen alt-title wins for display.
- Covers: cap 5 candidates (MangaDex catalogue/MangaUpdates 1). Chosen cover overlays library grid + /api/library.
- Detail layout fix 607ea9e: unclosed header/meta divs had nested chapters inside meta col (2102px header); closed + regen golden.
- Golden-file render lock top-level templates (decisions/template-golden-test) — catches typo'd partial includes / dropped context keys at build time.
- openspec detail-enrichment-fields: complete 26/26, validate --strict OK. ctest 15/15 (templates test adds one).
