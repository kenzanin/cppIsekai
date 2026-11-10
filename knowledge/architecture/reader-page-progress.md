---
type: decision
title: "reader-page: four-mode reader UI + persisted positions (COMPLETE, archived 2026-11-10)"
description: "reader-page complete. /cache/img page-context 404 FIXED (img_ensure context-miss now falls through to fetch; regression test in test_http_img). chapter_number now persists (reader posts it; route keeps stored when absent) unlocking worker v3 placement + resume hint. Worker nudge carries WindowCtx (page_base chapter-absolute, aligned with route query-page) and rel_path gained manga/ per cache page_rel -- one file per slot, no dupes. New [log] level knob (--log-level / CPPISEKAI_LOG_LEVEL / ini) + debug traces stale/cache/worker/position. 14/14 ctest. 4.1 browser smoke passed; change archived 2026-11-10."
tags: [reader-page, reader, position-persistence, cache-img-fix, logging, openspec, in-progress]
generated: { by: agent/mcp, at: "2026-11-09T18:56:55Z" }
status: stable
---

## State

reader-page complete and archived 2026-11-10. Smoke passed: four modes switch, resume-after-restart restored position, detail page continue-hint ok.

## Key facts

- Boot seeding precedence in reader.mustache boot(): server-stored position -> query ?ch/?page/?mode/?dir overrides -> JS defaults (ch 1, paged rtl).
- POST /reader/position doubles as prefetch-worker nudge (D4); 300ms debounce coalesces rapid paging + mode switches.
- Chapters resolved number->id client-side at boot (wrinkle: /api/pages takes chapter NUMBER, reader needs chapter ID for position POST; fetches /api/chapters in parallel).
- MangaDex quirk: some chapters (Solo Leveling tested) return empty at-home data[] upstream -- daemon correctly errors; not a daemon bug. Kage no Jitsuryokusha (77bee52c-d2d6-44ad-a33a-1734c1fe696a) works: 37 pages ch1.
- Page files land img/<plugin>/manga/<slug>/chNNN-<id8>/pNNN.<ext> (min-3 pad) only when /cache/img carries full context (plugin, slug, ch>=1, page>=1); no context = sha fallback.
- OPEN BUG found in live smoke: /cache/img WITH page context 404s while sha-fallback (no context) 200s on the same URL -- img_ensure context-miss branch returns nullopt BEFORE fetch when prefix not in entries_ (cache.cpp ~line 296); fetch-then-store path never reached with context. Fix before 4.1 smoke passes.
- detail.mustache unreachable from reader back-arrow (3.2 replaced route); revisit when detail continue-hint lands.
- Serena verdict: useful for C++ (diagnostics/find refs), useless for .mustache (clangd treats it as C++).
- coder-worker subagent (global, model 9router/free) delivered 3.1/3.2/3.3 successfully.

## Next

1. Fix /cache/img page-context 404 (img_ensure context branch must fall through to fetch on miss).
2. 4.1 live smoke: four modes in browser, resume after restart (restart-resume already verified via curl: boot seeds stored 1/7/spread-h/rtl), curl prefetch check pages land ch<n>-<id8>/.
3. Check 4.1 checkbox, openspec archive.

# Related Concepts
- [cache-layout-v3: per-plugin layout, global transcode, host cover ratio](cache-layout-v3.md): reader-page pages land in the v3 layout img/<plugin>/manga/<slug>/chNNN-<id8>/pNNN.<ext>; the open /cache/img page-context 404 bug lives in img_ensure introduced by v3.

## Resolved 2026-11-10 (bug-fix session)

- `/cache/img` page-context 404: `img_ensure` context-miss returned nullopt BEFORE fetch; now falls through to fetch (sha-lookup deliberately skipped per D2). Regression test in `tests/test_http_img.cpp` ("page-context miss fetches and lands in the v3 tree").
- `chapter_number` was never populated (0 forever): reader POSTs it (`chapter_number` field), route stores body value / keeps stored on absent. Feeds resume hint + worker v3 placement.
- Worker nudge had no `WindowCtx` (sha-fallback landing): position + skip routes now pass `{plugin, slug, ch, page_base=pos.page+1}`; `page_base>0` => chapter-absolute page numbers matching the route query-page convention (one file per slot, ext-free hit, CBZ-friendly).
- Worker `rel_path` lacked the `manga/` segment vs `cache::page_rel`: fixed + `test_worker` expectations (`plugin/manga/slug/chNNN-id8/pNNN`).
- New knob `[log] level` (registry entry 13: `--log-level` / `CPPISEKAI_LOG_LEVEL` / ini; boot-time, bad value keeps info) + debug traces: stale layer, cache img (ctx/sha hit-miss, fetch, stored+bytes), worker per-file, position route.

Live: position p12 => worker prefetch p013-p022 into `img/mangadex/manga/<slug>/ch001-<id8>/pNNN`; `/cache/img` ctx 200 webp, disk hit on repeat. ctest 14/14, zero new warnings.

Leftovers (non-blocking): skip-window page_base off-by-skip (dense numbering, order ok); detail.mustache back-arrow; legacy rows cn=0 self-heal on next POST.

Done: 4.1 checkbox, spec synced to openspec/specs/reader-page/spec.md, archived to openspec/changes/archive/2026-11-10-reader-page. detail.mustache path parse fixed (plugin=parts[2]).
