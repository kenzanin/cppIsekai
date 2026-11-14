# Tasks

## 1. Bit layout and store composition

- [x] 1.1 Define `enum class StatusFlag : unsigned` (bit0–bit6 via `1u << n`, int conversion through `std::to_underlying`) in `src/store/store.hpp`, then add a `status_category_mask(std::string_view)` helper in `src/store/store_search.cpp`: lowercase, `ongoing→kOngoing`, `hiatus→kHiatus`, `canceled|cancelled→kCanceled`, `completed→kCompleted`, else `0`.
- [x] 1.2 Add a file-local `compose_flags(entity)` helper building `category_mask | (done ? kDone : 0) | (is_new && !done ? kNew : 0) | (previous_flags & kDim)`; route `saved_stats_update` and `saved_read_rollup` through it. No new `<bit>` include — its helpers do not apply to mask composition.
- [x] 1.3 In `saved_stats_update`, keep `is_new` as "previously set OR grew in this call"; drop the `has_update` write.
- [x] 1.4 In `saved_clear_update`, keep clearing bit0 only (`&= ~1`) so category and dim bits survive.
- [x] 1.5 Update the `status_flags` bit comments in `src/store/store.hpp` and `src/store/store_internal.hpp` to the full layout.

## 2. Scheduler New-badge fix (G1)

- [x] 2.1 In `src/http/api_ctx.hpp` `sync_saved_stats`, compute `total` from the chapters envelope before the detail branch.
- [x] 2.2 Call `store.saved_stats_update(plugin, slug, "", total)` when the detail envelope is absent (chapters-only), keeping the existing detail-path call unchanged.
- [x] 2.3 Confirm an absent saved row stays a no-op (no crash) on the scheduler path.

## 3. Drop `has_update` (BREAKING)

- [x] 3.1 Remove the `has_update` field from `SavedManga` (`store.hpp`) and from `SavedMangaEntity` (`store_internal.hpp`); drop its column mapping.
- [x] 3.2 Remove `has_update` assignments in `saved_row`, `saved_list`, `saved_get`, and `save_manga` (`store_search.cpp`, `store_api.cpp`).
- [x] 3.3 In `src/store/store_core.cpp`, run the migration as an ordered sequence, each step guarded by `pragma table_info` so it is idempotent: (a) add `status_flags` if absent; (b) backfill every row from `has_update`/`total_ch`/`read_ch`/`status` while preserving bit6 (`status_flags & 64`); (c) `ALTER TABLE saved_manga DROP COLUMN has_update`. Add a `ponytail:` note that `DROP COLUMN` needs SQLite ≥ 3.35. Rationale: the live DB has no `status_flags` yet and 18/31 rows carry `has_update = 1` — dropping first would erase the New state.
- [x] 3.4 Drop the `has_update` JSON field in `src/http/http_api_library.cpp` and `src/http/http_pages.cpp`.
- [x] 3.5 Drop `data-update` from the grid card in `templates/grid_body.mustache`.

## 4. Dim toggle onto the row

- [x] 4.1 Add `store.saved_dim_update(plugin, slug, on)` — sets or clears bit6 only, no-op when the row is absent.
- [x] 4.2 Add `store.saved_dim_list()` returning the `(plugin, slug)` pairs whose bit6 is set.
- [x] 4.3 Rewire `POST /api/display/dim-blur` and `GET /api/display/dim-list` in `src/http/http_api_state.cpp` onto the store; retire `kDimPrefix` and the `plugin_state` keys.
- [x] 4.4 Leave `host/skip/` and `host/read/` chapter state untouched.

## 5. Templates

- [x] 5.1 `templates/detail.mustache`: add category badges gated on bits 2–5 and a dim badge/state gated on bit6, mirroring the existing New/Done spans.
- [x] 5.2 `templates/grid_body.mustache`: switch the Complete filter from the `data-status` string match to bit5 (`status_flags & 32`).
- [x] 5.3 `templates/detail.mustache`: drop the undefined `progressPct || isDone` reference.

## 6. Tests

- [x] 6.1 `tests/test_store.cpp`: lifecycle subcase — fresh save (no bits) → growth sets bit0 → `read_ch >= total` sets bit1 and clears bit0 → each known status sets exactly its category bit (`cancelled` folds to canceled) → unknown status sets none → dim toggle sets/clears bit6 → stat writers preserve bit6 → `saved_clear_update` clears bit0 only.
- [x] 6.2 `tests/test_store.cpp`: rewrite the existing `has_update` assertions to read bit0.
- [x] 6.3 Scheduler scenario: chapters-only `sync_saved_stats` with a grown chapter list sets `total_ch` and bit0; an unchanged count leaves bit0 alone.
- [x] 6.4 `tests/test_templates.cpp`: detail page renders each badge only when its bit is set; end-to-end `inject_status_flags` coverage (data object gains the integer; non-object envelope untouched).
- [x] 6.5 Re-lock the detail-page golden (`CPPISEKAI_UPDATE_GOLDEN=1`), then re-run without it and confirm only badge markup moved.

## 7. Build and verify

- [x] 7.1 `xmake -y` clean (zero-warning gate `-Wall -Wextra`).
- [x] 7.2 `xmake test test_store`, `xmake test test_templates`, then full `xmake test`.
- [x] 7.3 `codebase-memory-mcp/index_repository` after the structural store/http edits.
