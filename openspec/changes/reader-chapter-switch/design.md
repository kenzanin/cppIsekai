# Design

## Context

The reader is a single Alpine component (`readerPage()` in `templates/reader_script.mustache`) that boots from mustache-seeded resume state (`{{resume_ch}}`/`{{resume_page}}`/`{{resume_mode}}`/`{{resume_dir}}`/`{{resume_zoom}}`) plus `?ch/?page/?mode/?dir/?zoom` query overrides, then fetches `/api/pages/<plugin>/<slug>/<ch>` (page URLs + `prev_ch`/`next_ch`) and `/api/chapters/<plugin>/<slug>` (chapter list, used to resolve `chId` for `POST /reader/position`). Chapter changes today go through `window.location = this.chUrl(...)` (in `step()` at boundaries, `gotoChapter()`, and the dropdown), causing a full reload. Lazy loading is two mechanisms: an `IntersectionObserver` (`observe()`, webtoon) and neighbour `preload()` (paged/spread), both keyed off the `pages` array and `seen` list. See proposal.md — Why.

## Goals / Non-Goals

**Goals:**
- Chapter switches swap content in place; same component instance survives (mode/dir/zoom/`seen` bookkeeping re-armed, not re-created).
- URL stays reload/back/forward-correct via `pushState`/`popstate`.
- Prefetch adjacent chapters so boundary crossings don't wait on the network.

**Non-Goals:**
- No server/API changes (existing JSON endpoints suffice).
- No prefetch of page *images* beyond the current chapter's existing lazy loading — only chapter page *lists* are prefetched.
- No virtualization/windowing of long chapters; existing lazy-load behavior is unchanged within a chapter.

## Decisions

- **`loadChapter(n, page)` extracted from `boot()`'s fetch half; `boot()` keeps the mustache-seed/query-parsing half.** The fetch + `chId` resolve + observer setup is the only part an in-place switch needs. Alternative: a full `reload()` that re-runs `boot()` — rejected because mustache seeds are baked at render time and would override live state.

- **Prefetch cache = `Map` keyed by chapter number, capped at 2 entries (next/prev), holding the `/api/pages` JSON.** On switch, `loadChapter` checks the cache first and evicts entries outside the ±1 window. Alternative: `Cache-Control`/service worker — rejected as heavyweight and unnecessary when only two adjacent lists matter. The chapter list `/api/chapters` is fetched once at boot and kept; only the pages endpoint is refetched per chapter.

- **`history.pushState` on every chapter switch (and page change within a chapter keeps its current behavior of query-less URL via `chUrl`), one `popstate` handler that calls `loadChapter` for the chapter in the URL (or only scrolls/sets `page` when the chapter is unchanged).** Alternative: `replaceState` only — rejected: spec requires back to return to the previous chapter.

- **Boundary crossing logic stays in `step()`; only the `window.location` assignments become `loadChapter(this.prev_ch, lastPageOfPrev)` / `loadChapter(this.next_ch, 1)`.** `gotoChapter(n)` becomes `loadChapter(n, 1)`. The dropdown in `reader_top.mustache` calls the same path (its current markup-level handler change only).

- **On in-place switch: reset `pages`, `seen`, disconnect+recreate the `IntersectionObserver`, `preload()` after `$nextTick`, and `_resetScroll()`; preserve `mode`, `dir`, `zoom`, `zoomPct`, `chaptersList`.** Same re-arm sequence `modeChanged()` already uses — reuse that pattern rather than a second mechanism.

- **Prefetch runs after render via `$nextTick`, fetching next first then prev sequentially** (two small JSON calls; sequential keeps the bandwidth claim in the proposal and avoids competing with the render's image lazy-loads).

## Risks / Trade-offs

- [Race: user switches chapters faster than fetches resolve] → `loadChapter` guards with a monotonically increasing request token; stale responses drop out.
- [Stale prefetch data if the server's chapter list changes mid-session] → prefetched lists are only page URLs + chapter numbers from the same endpoints a reload would hit; worst case a switch uses a slightly stale list the same way a cached page would. No mitigation needed beyond the ±1 eviction.
- [`popstate` during an in-flight switch] → same request token guard; the latest navigation always wins.
- [Position POST during switch (`persist()` debounce pending while `chId` changes)] → `save()` already snapshots at fire time and posts `chapter_number: this.ch`; flush or clear the pending timer in `loadChapter` before changing `ch`/`chId` so a position is never posted against the wrong chapter.

## Migration Plan

No deploy/rollback concerns: client-side template change only. Golden fixtures regenerate (`CPPISEKAI_UPDATE_GOLDEN=1`) since the reader shell ships inside rendered pages.

## Open Questions

None.
