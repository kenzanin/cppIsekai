---
type: Fact
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Detail page + enrichment (shipped 2026-11-11, openspec complete 26/26): recommendations kind (MangaDex/MangaUpdates, cap 5) merged into Related; fetched entries keyed (source,kind); chosen cover overlays library grid + /api/library; covers cap 5 (MangaDex catalogue/MangaUpdates 1); tags attributes.tags; MU Title-Case; host.log.debug; combined alt-titles picker (MangaDex alTitles + MangaUpdates associated, unicode-aware dedupe; single picker from picker modal); detail template layout restructured (unclosed header/meta divs fixed 607ea9e -> chapters were nested in meta col, 2102px header; golden regen 2d91dcf); golden-file render lock in place (decisions/template-golden-test). Daemon :8090."
tags: [detail, enrichment, cover, wizard, cache, plugin]
generated: { by: agent/mcp, at: "2026-11-11T08:14:38Z" }
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
