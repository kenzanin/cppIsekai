# Design: refactor-hygiene

## D1. Split by writer, not by line count

`store_search.cpp` mixes four concerns that change for different reasons: FTS search,
`saved_*` stat writers, dim, and library listing. The split boundary follows the writer:
search + listing read, the `saved_*` family writes.

**The constraint that shapes it:** `compose_flags` must remain the single place that builds
the `status_flags` integer (`architecture/status-flags-bitfield`). Splitting the writers
across files is fine; duplicating the composition is not. If a split would force a second
copy of `compose_flags`, that split is wrong.

## D2. Same rule for enrich

`enrich_internal.hpp` is split by request concern, not by moving the largest function out.
The 200-line target is a reviewability guideline, not a quota — a split that leaves two
files needing each other's internals has moved the problem, not solved it.

## D3. Extracted template scripts served through the existing `.br` route

A4 does not need new serving code. `br_asset()` already serves `static/*.br` with a plain
fallback, which is how Tailwind, Alpine and htmx are delivered today. The extracted
`static/detail.js` and `static/reader.js` join that route.

**Why this is worth doing at all:** inline Alpine cannot be linted, cannot be unit-tested,
and makes every logic edit a template-golden edit. Extraction turns template churn into a
static-asset change.

**Cost accepted:** one more request per page, and the browser smoke check becomes mandatory —
moving working code is exactly the change that a green test suite cannot vouch for.

## D4. Golden re-lock becomes two steps

Probed behaviour, current state: editing a template without re-locking fails `test_templates`
(2 failed assertions) — good. But `CPPISEKAI_UPDATE_GOLDEN=1 <test binary>` rewrites the
fixture and passes in the same invocation, so the escape hatch both detects and erases the
signal.

**Decision.** Keep the env var (it is the only way to re-lock) but make the check refuse to
pass silently: when `CPPISEKAI_UPDATE_GOLDEN` is set, the run SHALL report which fixtures it
rewrote and SHALL fail unless a second, explicit acknowledgement is present. Re-locking then
requires (1) run with update, (2) read `git diff tests/fixtures/golden/`, (3) run again
without it.

**Alternative rejected.** Deleting the env var and hand-editing fixtures — it removes the
only automated path and pushes the work into copy-paste, which is how a wrong golden becomes
a committed golden.

## D5. Verification order

Each split is its own commit: suite green, split, suite green again, goldens unchanged. A
split and a behaviour fix in one commit makes the diff unreviewable and hides the fix.
