---
type: Fact
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Enrichment shipped state 2026-11-11: title_meta single-row + chosen merge (apply_chosen), enrichment routes (GET detail/covers cached-or-force, POST chosen/reset, /api/enrichment listing), chosen whitelist (exact-key, artist/author String|List), detail page picker UI; pending 4.2 modal + group5 tests/group6 gates."
tags: [detail, enrichment, cover, wizard, cache, plugin]
generated: { by: agent/mcp, at: "2026-11-11T03:29:54Z" }
status: stable
---

## Enrichment architecture facts (2026-11-11 user direction)
- Enrichment = dedicated plugins, **separate path**: `./data/enrichment/<id>/*` (NOT `./data/plugins`). Same plugin machinery (Lua/sol2, http.fetch, storage.*, log.*, ApplyProxy); only directory capability (`enrichment`) differ. Sources first: MangaDex + MangaUpdates (official JSON APIs), more later.
- Two fetch paths, split: **fetch detail** (title, synopsis, artist, author, genre/tags) vs **fetch covers** (candidate covers only far heavier).
- Trigger: explicit user buttons on library detail page (saved manga only). Never implicit.
- **Original** `saved_manga` (name/cover, captured add-to-library) + stale-cached source-plugin detail in `api_cache`. **Never overwritten** → Reset cover/detail revert source free.
- **title_meta** single row per (plugin, slug): `fetched` (array raw payloads per source, incl. provenance-at fetch time), `chosen` (user overrides: {title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[]}; values copied, no runtime provenance). Display = merge `chosen.<field> ?? source cache`. Reset detail = clear `chosen` non-tag/cover fields; Reset cover = clear `chosen.cover`.
- **Cache model**: `fetched` ordered per-source usability. Matching may be loose — user decides data to use; nothing auto-applied.
- Matching: no forced auto-commit; uncertain matches presented as candidates user chooses from.

## Shipped state (2026-11-11, openspec change detail-enrichment-fields, groups 1-4 partial)
- Store: `title_meta` table + read/merge/replace write path (read-merge-replace; sqlite_orm insert always throws NOT NULL — use replace). Corrupt-row test covered (49f4077, 4a63f00).
- Routes (3bcfea1, 0ab0d88, 62858ec, 813342c): GET enrichment detail/covers (cached-or-fetch + force), POST chosen, POST reset; /api/enrichment lists loaded enrichment sources; /api/detail envelope merges title_meta.chosen via `apply_chosen` in src/http/enrich_internal.hpp. chosen whitelist = exact keys only; tags must be strings (62858ec); artist/author accept String|List (4a73c3a fix).
- UI (4a73c3a): detail.mustache enrichment panel — fetch detail/covers buttons, candidate picker (Alpine), reset cover/detail next to cover. Saved-manga gate enforced server-side.
- tasks.md 3.1-3.5 done (a291fb7); 13/25 tasks complete + group4 partial.
- **Pending**: 4.2 cover wizard modal overlay + Reset cover button placement; group 5 tests (tests/test_http_enrich.cpp); group 6 gates (ctest, openspec validate --strict, okf persist + browser live smoke); 5.3 delta sync. Daemon on :8090 may run older binary — restart to pick up commits.
- Stash `wip-pre-tidy-review` parked untouched. Branch cppIsekai-4 merge attempt was no-op (already in main).
