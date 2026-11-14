---
type: decision
title: cppIsekai image-cache + offline-first pathway
description: "cppIsekai image-cache + offline-first pathway -- cache-layout sub-change COMPLETE 2026-11-08 (9/9 tasks, openspec all_done, ctest 10/10 green, live smoke verified legacy purge + CBZ export)."
tags: [rewrite, cache, offline, proxy, stale, reader, mangadex, cache-layout, cbz-export]
generated: { by: agent/mcp, at: "2026-11-14T10:31:33Z" }
status: stable
---

Decision: change cache-layout (branch cache-layout) mid-flight 2026-11-08 -- progress 4/9 tasks verified (all [unverified] subagent receipts independently re-verified by controller: ctest full 9/9 + tree clean + diff-scope read each time).

Landed & committed:
- a6fce07 cache: slug pragma header-only (fs-safe lowercase, 64-char UTF-8-safe cap, collision suffix last-8-chars-of-manga-id, NO DB) + tests/test_slug.cpp.
- c5ad5a0 cache: layout v2 -- rel_path trailing optional param on ensure/store/peek (img-only; byte-identical sha fallback when empty); boot scan switched to RECURSIVE whole-root walk (cross-pool mtime LRU); purge-once gated by .cache-v2 stamp + pure-sha-flat detection, rename <root>/img -> .legacy-img-<hex> then next-boot sweep.
- 65730d7+cbf2f6e+6a529e0 worker: reader::rel_path(WindowCtx) -> <plugin>/<slug>/chNNN/pNNN.<ext> (zero-pad 3+3, ext sanitized mirror cache::ext_of); missing slug/chapter => bitwise sha fallback; http.cpp nudge call sites untouched (defaulted param).
- d0581d1 chore(openspec): tick tasks 1.1-1.3 (openspec apply 4/9 = 1.1,1.2,1.3,2.1 [latter self-tick at delivery]).
- f66ae74 docs: refresh AGENTS.md.

Git rectification (user-ruled): branch cli-config had cache-layout capture at its tail (b5a6afe); cli-config merged to main via ff80746 --no-ff (capture included, harmless), branch deleted; rebase --onto main b5a6afe => cache-layout is now 2 work commits + ticks directly on main.

Pending: 2.2 /reader/window probe (verify worker structured writes when slug present), 3.1 vendored libzippp + CMake, 3.2 GET /export/cbz streaming route, whole-diff independent review (R1'), 4.1 ctest+openspec validate, 4.2 live smoke (purge message in log + CBZ download + structured window probe).

Rulings: R1' independent review ONCE whole-diff before 4.1; R2 serial workers; R5 SDD workspace files English; R6 libzippp vendored under third_party/libzippp, minizip build-level FALLBACK only.

# Related Concepts
- [cache stale-hit guard: wipe no longer serves 200-blank](../decisions/cache-stale-hit-guard.md): Guard rules for serving stale cache hits.
- [api_cache db-lock treated as cache miss: local-first regression (mitigated)](../decisions/api-cache-lock-miss-regression.md): Local-first regression from db lock read as cache miss.
- [api_cache NULL payload served as 200-empty-body](../decisions/api-cache-null-payload.md): Null-payload handling in the api cache.
- [cover prefetch: dedicated worker, cache-first, library + search hooks](cover-prefetch-worker.md): Background cover prefetch worker.
- [Library reads are local-first](local-first-reads.md): Local-first read path.
