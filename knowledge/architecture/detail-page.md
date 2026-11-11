---
type: Fact
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Enrichment shipped+applied 2026-11-11: recommendations kind (MangaDex/MangaUpdates, cap 5) merged into Related; fetched entries keyed (source,kind); chosen cover overlays library grid + /api/library; covers cap 5 (MangaDex catalogue/MangaUpdates 1); tags from attributes.tags, MU Title-Case; host.log.debug added."
tags: [detail, enrichment, cover, wizard, cache, plugin]
generated: { by: agent/mcp, at: "2026-11-11T05:23:57Z" }
status: stable
---

## Enrichment architecture facts (2026-11-11 user direction)
- Enrichment = dedicated plugins, **separate path**: `./data/enrichment/<id>/*` (NOT `./data/plugins`). Same plugin machinery (Lua/sol2, http.fetch, storage.*, log.*, ApplyProxy); only directory capability (`enrichment`) differ. Sources first: MangaDex + MangaUpdates (official JSON APIs), more later.
- Two fetch paths, split: **fetch detail** (title, synopsis, artist, author, genre/tags) vs **fetch covers** (candidate covers only far heavier).
- Trigger: explicit user buttons on library detail page (saved manga only). Never implicit.
- **Original** `saved_manga` (name/cover, captured add-to-library) + stale-cached source-plugin detail in `api_cache`. **Never overwritten** → Reset cover/detail revert source free.
- **title_meta** single row per (plugin, slug): `fetched` (JSON array, **entries keyed (source, kind)** — `kind` ∈ detail|covers|recommendations; missing = legacy detail entry, served for `detail` only), `chosen` (user overrides: {title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[]}). Display = merge `chosen.<field> ?? source cache`. Reset detail = clear `chosen` non-tag/cover fields; Reset cover = clear `chosen.cover`.
- **Recommendations** (2026-11-11): third ABI kind `recommendations` (cap 5, weight/score-ordered). MangaDex: `/manga/<id>/recommendation` scored pairs. MangaUpdates: `recommendations` list on `/series/<id>` payload (v1 has NO art/gallery or multi-cover endpoint on key-free plan). UI merges scraper related + both enrichment sources, dedupe by URL, into the "Related" section.
- **Covers cap**: MangaDex catalogue `/cover?manga[]=<id>` capped at 5 (primary first, then locale en, ja, rest). MangaUpdates: single cover (`image.url.original`).
- **Chosen cover overlay** (commit 2a79ca5): `chosen.cover` merged everywhere saved covers render — home/library grid SSR + `/api/library` (helper `http::api::chosen_cover()` in `src/http/enrich_internal.hpp`), not just `/api/detail`.
- **Tags**: MangaDex tags from `attributes.tags` (genre|theme|format); MangaUpdates genres Title-Cased like MangaDex (connector words lowercase; `Sci-Fi` kept).
- **host.log.debug** (src/plugin/lua/lua_runtime.cpp): new debug level (default `info`); enrichment fetch logs emit at debug; daemon may run with `--log-level debug`.
- `normalize_chosen()` extracted into `enrich_internal.hpp` (unit-tested via test_http_enrich).
- Verified live (browser + direct endpoint): fetch detail/covers/recommendations OK, force refetch, tag toggle into `tags_add`, pick cover, Reset cover / Reset detail endpoints return `{ok:true}` and clear chosen. Daemon `:8090`.
- openspec change detail-enrichment-fields: tasks 24/25 (only 6.3 pending at time of write); design.md carries "Post-apply amendments" section. ctest 15/15 green; openspec validate --strict OK; okf conformant (gate_passed:false due 14 pre-existing orphans, not a regression).
