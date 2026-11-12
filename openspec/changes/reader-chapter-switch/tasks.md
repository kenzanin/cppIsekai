# Tasks

## 1. Refactor boot into reusable chapter load

- [ ] 1.1 In `templates/reader_script.mustache`, split `boot()` into seed/query parsing (stays in `boot()`) and a new async `loadChapter(n, page, {push = true})` that fetches `/api/pages`, resolves `chId` from `chaptersList`, resets `pages`/`seen`, re-arms the `IntersectionObserver`, `preload()`s after `$nextTick`, and `_resetScroll()`s — verify: `xmake build` renders the reader and loading a chapter via URL (fresh load) still behaves identically to today.
- [ ] 1.2 Flush/clear the pending `persist()` timer inside `loadChapter` before mutating `ch`/`chId` (per design: no position POST against the wrong chapter) — verify: rapid chapter switches post positions only for chapters actually rendered (manual check or added assertion in reader self-check).
- [ ] 1.3 Add the monotonic request-token guard in `loadChapter` so stale responses drop out — verify: spamming next/prev across chapter boundaries always lands on the last requested chapter.

## 2. In-place navigation

- [ ] 2.1 Replace `window.location = this.chUrl(this.prev_ch, ...)` / `(this.next_ch, 1)` in `step()` with `loadChapter(this.prev_ch, lastPageOfPrev)` / `loadChapter(this.next_ch, 1)`; `lastPageOfPrev` comes from the prefetched pages list length (fallback: full `loadChapter` fetch) — verify: boundary crossings swap content without document reload (no flash, Alpine state survives).
- [ ] 2.2 Change `gotoChapter(n)` to `loadChapter(Number(n), 1)` — verify: keyboard/dropdown chapter jumps stay in place.
- [ ] 2.3 Update the chapter dropdown handler in `templates/reader_top.mustache` to call the in-place switch (markup-level change only) — verify: dropdown selection switches chapter in place and reflects the current chapter.
- [ ] 2.4 `history.pushState` with `chUrl(n, page)` on every successful in-place switch; add a `popstate` handler that calls `loadChapter` for the URL's chapter (only re-set `page` when the chapter is unchanged) — verify: back/forward restore the correct chapter+page in place; refresh after an in-place switch deep-links to the same chapter+page.

## 3. Adjacent-chapter prefetch

- [ ] 3.1 Add the ±1 prefetch cache (`Map` keyed by chapter number, evict outside ±1) checked first by `loadChapter` — verify: switching to a prefetched chapter issues no `/api/pages` fetch (network tab / logged fetch count).
- [ ] 3.2 After a chapter renders, prefetch next then previous sequentially after `$nextTick` — verify: both adjacent page lists land in the cache; switching into either is instant.

## 4. Regression and fixtures

- [ ] 4.1 Run `xmake test` (doctest suite incl. `test_templates`) — verify: all existing tests pass unchanged.
- [ ] 4.2 Regenerate golden fixtures (`CPPISEKAI_UPDATE_GOLDEN=1` run of the template tests) and review the diff is limited to the reader shell script/markup — verify: `git diff tests/fixtures` shows only the intended reader changes, then `xmake test` passes on the regenerated goldens.
