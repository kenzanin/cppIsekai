---
type: Fact
title: "reader: read-ahead cross-chapter prefetch (worker + 10-page window)"
description: "Prefetch after a recorded position already spills across chapter boundaries: on /reader/position the window is re-run (skip-aware) starting at pos.page+1 in the active chapter and continues into following chapters until the page budget (default limit 10) fills; the reader::Worker prefetches it in order via cache::ensure/img_ensure. Empty window when position is unrecorded."
tags: [reader, prefetch, worker, window, cache-img]
generated: { by: agent/mcp, at: "2026-11-10T03:42:56Z" }
status: stable
---

## Mechanism
`next_window` (src/reader/window.cpp) seeds the sliding prefetch window: chapters sorted by number, walking from the position chapter onward, skipping chapters flagged in plugin_state, contributing only pages AFTER the current one in the position chapter; spills into the next chapter(s) until `limit` (default 10) urls collected. The `/reader/position` route re-runs it on every position POST and nudges reader::Worker (src/reader/worker.hpp) with the resulting list. Worker is one thread, woken only by a nudge (coalescing: newest list wins) — no timer. It walks the list in order through cache::ensure / img_ensure (transcode to [cache] image_format), sequential on the shared img pool with /cache/img, concurrent-miss dedup included. rel_path(WindowCtx{plugin, slug, chapter, page_base}) maps urls to v3 layout paths so worker and /cache/img land one file per slot.

## Boundary semantics
- Only pages after the CURRENT one in the position chapter are windowed (pos.page begin); the current page itself is not re-fetched.
- Skipped chapters contribute no pages (keep walking).
- Unrecorded position (no stored row / empty chapter_id) yields an EMPTY window — no prefetch until the first POST /reader/position after the reader boots. Read-ahead therefore lights up only after the reader has posted its first position.
- page_base: chapter-absolute page (batch[0] page_base) keeps /cache/img and worker placements consistent.

## Verified
Code read + unit tests (test_window, test_worker) green; 14/14 ctest. NOT live-verified on daemon (window prefetch is silent).

# Related Concepts
- [reader-fullscreen](../architecture/reader-fullscreen.md): reader UI/chrome; preloads current+neighbour views client-side, complements the worker.
- [reader-page](../architecture/reader-page-progress.md): base reader capability; position persistence it extends.
