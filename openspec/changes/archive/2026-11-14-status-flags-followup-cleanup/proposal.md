# Proposal: status-flags-followup-cleanup

## Why

Per-manga state on `saved_manga` is spread across a derived `status` string, a redundant `has_update` column, and a `plugin_state` key for the dim/blur toggle. `status_flags` already encodes bit0 (is_new) and bit1 (is_done) with free bits reserved for exactly this purpose. Consolidating the remaining state into that single integer gives the client one thing to gate on, removes a duplicated column, and makes status classification a stored bit instead of a string comparison repeated in every consumer.

This change also closes the followup gaps recorded after c0fce4e: the auto-update scheduler never advances the New badge, and the flag lifecycle, `inject_status_flags`, and the detail-page badge bits have no tests.

## What Changes

- **Scheduler New-badge fix (G1):** `sync_saved_stats` runs the `total_ch` / bit0 update in the `chapters_only` path too, so a background auto-update marks a saved entry New when its chapter list grew. No extra plugin call — the scheduler already fetches `chapters`.
- **One-hot status category bits:** `saved_stats_update` derives `bit2=ongoing`, `bit3=hiatus`, `bit4=canceled`, `bit5=completed` from the source `status` string (case-insensitive; `cancelled` folds to canceled). The raw `status` column stays as the display source (detail chip); the bits are the derived, queryable classification.
- **Dim toggle → bit6:** the dim/blur cover toggle moves from `plugin_state` key `host/dim/<plugin>/<slug>` into `status_flags` bit6 via a new `store.saved_dim_update(plugin, slug, on)`; `GET /api/display/dim-list` becomes a `status_flags & 64` scan over saved rows.
- **Drop `has_update` column (BREAKING):** `has_update` duplicates bit0. The column is dropped (`ALTER TABLE saved_manga DROP COLUMN has_update`), `status_flags` bit0 becomes the single New source, and the `has_update` field is removed from `GET /api/library` and the grid's `data-update`.
- **Detail-page badges (G4):** the detail page renders category badges (Ongoing/Hiatus/Canceled/Complete) and the dim state from the stored bits, consistent with the existing New/Done badges; badges never depend on client-side computed progress.
- **Store-level flag-lifecycle test (G2):** one `test_store` subcase covering `saved_stats_update` → `saved_read_rollup` → `saved_clear_update` → `saved_dim_update`, including bit6 preservation across the stat writers.
- **`inject_status_flags` test:** focused unit test that the helper writes the integer into the envelope's data object.
- **Dead-ref cleanup:** drop the undefined `progressPct || isDone` reference in `detail.mustache`.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `library-save` — `status_flags` becomes the single per-manga state integer (New/Done/category/Dim); `has_update` leaves the stat-field list and the API response; Complete filtering and the dim toggle gate on bits.
- `auto-update` — a chapters-only refresh advances `total_ch` and bit0 for saved entries.

## Impact

- **Store:** `src/store/store_search.cpp` (flag composition, `saved_dim_update`, `saved_dim_list`), `src/store/store_internal.hpp` + `src/store/store.hpp` (drop `has_update` field, add dim accessors), `src/store/store_core.cpp` (DROP COLUMN migration).
- **HTTP:** `src/http/api_ctx.hpp` (chapters-only stat sync), `src/http/http_api_library.cpp` + `src/http/http_pages.cpp` (drop `has_update` in JSON/context), `src/http/http_api_state.cpp` (dim routes onto the store).
- **Templates:** `templates/detail.mustache` (category + dim badges, dead-ref), `templates/grid_body.mustache` (Complete filter on bit5, drop `data-update`).
- **Tests:** `tests/test_store.cpp` (flag lifecycle, dim bit, rewritten `has_update` assertions), `tests/test_templates.cpp` (detail badges, `inject_status_flags`), plus a scheduler stat-advance check.
- **DB:** SQLite `DROP COLUMN` (needs ≥ 3.35; repo builds against 3.54). Existing rows need no backfill — bit0 already mirrors `has_update`.
- **API:** **BREAKING** — `has_update` disappears from `GET /api/library`; dim on an unsaved manga becomes a no-op.
- **Risk:** the stat writers must preserve bit6 (and category bits from the stored `status`); covered by the lifecycle test. Detail-page SSR changes, so the template golden needs a re-lock.
