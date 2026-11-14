# Tasks: refactor-hygiene

No spec deltas: this change is behaviour-preserving. Verification is the existing suite plus
a reviewed golden diff.

## 0. Baseline

- [ ] 0.1 `xmake -y` clean and `xmake test` green before touching anything; record the
      baseline so a later failure cannot be blamed on the refactor
- [ ] 0.2 `git diff tests/fixtures/golden/` empty at baseline

## 1. A1 — split `src/store/store_search.cpp` (794 lines)

- [ ] 1.1 Group the file's contents by concern: FTS search, `saved_*` stat writers, dim,
      library listing
- [ ] 1.2 Split into units of roughly <=200 code-loc, header-only where the convention asks
      for it
- [ ] 1.3 Keep `compose_flags` the single place that composes `status_flags`; if a split
      would duplicate it, that boundary is wrong — choose another
- [ ] 1.4 `xmake test test_store` green, no golden change

## 2. A2 — split the enrich unit (`enrich_internal.hpp` 20.3K + `http_api_enrich.cpp`)

- [ ] 2.1 Split by request concern, not by extracting the largest function
- [ ] 2.2 Keep the public entry point unchanged so callers do not move
- [ ] 2.3 `xmake test test_http_enrich` green, no golden change

## 3. A4 — extract inline template scripts

- [ ] 3.1 Move the Alpine body of `templates/detail_script.mustache` (318 lines) into
      `static/detail.js` and the body of `templates/reader_script.mustache` (293 lines) into
      `static/reader.js`
- [ ] 3.2 Serve both through the existing `br_asset()` route, consistent with how
      Tailwind/Alpine/htmx are already delivered; no new serving machinery
- [ ] 3.3 Leave each partial as a script tag; re-lock goldens once and confirm the diff is
      limited to that script tag
- [ ] 3.4 Browser smoke: open a reader page and a detail page, exercise the pager and a
      chapter load, confirm behaviour is unchanged

## 4. A5 — make golden re-locking deliberate

- [ ] 4.1 Make a run with `CPPISEKAI_UPDATE_GOLDEN` report which fixtures it rewrote and
      fail unless an explicit second acknowledgement is present
- [ ] 4.2 Confirm the escape hatch can no longer turn a red suite green silently
- [ ] 4.3 Document the two-step re-lock (update, read `git diff`, re-run without the flag)
      where contributors will see it
- [ ] 4.4 Confirm a deliberate template edit without a re-lock still fails `xmake test`

## 5. Final verification

- [ ] 5.1 `xmake -y` clean with `-Wall -Wextra`
- [ ] 5.2 `xmake test` full suite green
- [ ] 5.3 No golden diff left over from A1/A2; the only golden commit is A4's, reviewed
- [ ] 5.4 One commit per item, each self-contained and reviewable on its own
