# Tasks: refactor-hygiene

No spec deltas: this change is behaviour-preserving. Verification is the existing suite plus
a reviewed golden diff.

## 0. Baseline

- [x] 0.1 `xmake -y` clean and `xmake test` green before touching anything; record the
      baseline so a later failure cannot be blamed on the refactor
- [x] 0.2 `git diff tests/fixtures/golden/` empty at baseline

## 1. A1 — split `src/store/store_search.cpp` (794 lines)

- [x] 1.1 Group the file's contents by concern: dedup/similarity core (fts, tokenize, dice,
      `recompute_dup_groups`), `saved_*` writer family (incl. `status_category_mask` /
      `compose_flags`), chapter flags/pages family, and the residue (plugin registry,
      search/count, `state_*`, `position_*`)
- [x] 1.2 Split into units of roughly <=200 code-loc, header-only where the convention asks
      for it
- [x] 1.3 Keep `compose_flags` the single place that composes `status_flags`; if a split
      would duplicate it, that boundary is wrong — choose another
- [x] 1.4 `xmake test test_store` green, no golden change

## 2. A2 — split the enrich unit (`enrich_internal.hpp` 20.3K + `http_api_enrich.cpp`)

- [x] 2.1 Split by request concern, not by extracting the largest function
- [x] 2.2 Keep the public entry point unchanged so callers do not move
- [x] 2.3 `xmake test test_http_enrich` green, no golden change

## 3. A4 — extract inline template scripts

- [x] 3.1 Move the Alpine body of `templates/detail_script.mustache` (318 lines) into
      `static/detail.js` and the body of `templates/reader_script.mustache` (293 lines) into
      `static/reader.js`
- [x] 3.2 Serve both through the existing `br_asset()` route, consistent with how
      Tailwind/Alpine/htmx are already delivered; no new serving machinery
- [x] 3.3 Leave each partial as a script tag; re-lock goldens once and confirm the diff is
      limited to that script tag
- [x] 3.4 Browser smoke: open a reader page and a detail page, exercise the pager and a
      chapter load, confirm behaviour is unchanged

## 4. A5 — make golden re-locking deliberate

- [x] 4.1 Make a run with `CPPISEKAI_UPDATE_GOLDEN` report which fixtures it rewrote and
      fail unless an explicit second acknowledgement is present
- [x] 4.2 Confirm the escape hatch can no longer turn a red suite green silently
- [x] 4.3 Document the two-step re-lock (update, read `git diff`, re-run without the flag)
      where contributors will see it
- [ ] 4.4 Confirm a deliberate template edit without a re-lock still fails `xmake test`

## 5. A6 — precompiled header for the daemon target

- [ ] 5.1 Include-census the daemon target's TUs; write `src/std.pch.hpp` with the frozen
      third-party roots plus std, no project headers (D6)
- [ ] 5.2 Plumb an opt-in `pch` field through `cxx_bin` and enable it for the daemon target
      only; if the clang path needs a forced include, add exactly that line per TU
- [ ] 5.3 Record clean-build and touch-`api_ctx.hpp` incremental times before/after in the
      commit message; revert the PCH if the clean-build win is not clearly measurable
- [ ] 5.4 `xmake -y` zero warnings, `xmake test` green, no behaviour change (PCH is
      additive); third-party targets and test binaries untouched

## 6. Final verification

- [ ] 6.1 `xmake -y` clean with `-Wall -Wextra`
- [ ] 6.2 `xmake test` full suite green
- [ ] 6.3 No golden diff left over from A1/A2; the only golden commit is A4's, reviewed
- [ ] 6.4 One commit per item, each self-contained and reviewable on its own
