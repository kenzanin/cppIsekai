# Design

## Context

`status_flags` on `saved_manga` is written by exactly three store methods, all in `src/store/store_search.cpp`:

- `saved_stats_update(plugin, slug, status, total_ch)` (617–644): stores `status`, raises `total_ch`, flags growth, then sets `e.status_flags = (done ? 2 : 0) | ((has_update && !done) ? 1 : 0)`.
- `saved_read_rollup(plugin, slug, read_ch, last_read_at)` (670–686): stores the read counter and recomputes the same two bits.
- `saved_clear_update(plugin, slug)` (688–705): stamps `last_read_at`, drops the update flag, and does `status_flags &= ~1`.

Each writer rebuilds the integer from scratch, so any new bit must be composed in every one of them. Today the value carries only bit0/bit1; `has_update` duplicates bit0 as a column, the status category lives only as a free-text `status` string, and the dim toggle lives outside the table in `plugin_state` (`host/dim/<plugin>/<slug>`).

Other grounded facts:

- `sync_saved_stats` (`src/http/api_ctx.hpp` 151–219) calls `saved_stats_update` only inside the detail-envelope branch, so the chapters-only scheduler path never advances `total_ch` / bit0.
- `inject_status_flags` (`src/http/api_ctx.hpp` 131–145) writes an integer into the envelope's data object; untested.
- Migration mechanism (`src/store/store_core.cpp`) is a guarded additive loop: `pragma table_info` → `ALTER TABLE ... ADD COLUMN` when absent, run once alongside the ORM connection.
- `templates/detail.mustache` gates New/Done on `detail.status_flags & 1` / `& 2`; `templates/grid_body.mustache` gates the card badge the same way but still string-matches `data-status` for the Complete filter.

## Goals / Non-Goals

**Goals:**
- One stored integer carrying New, Done, status category, and dim; every consumer gates on bits.
- Scheduler keeps the New badge correct without a `detail` call.
- `has_update` column gone; bit0 is the single New source.
- Lock the semantics with store, template, and helper tests.

**Non-Goals:**
- Removing the raw `status` column (it stays as the display source for the detail chip).
- Encoding `dup_group` as a bit (it is a group id, not a boolean) or chapter-level skip/read state (keyed per chapter, not per manga).
- Reworking the duplicate-cluster or reader-window logic.

## Decisions

**D1 — Bit layout (single `INTEGER`, bit0–bit6 used).**

| bit | value | meaning |
| --- | --- | --- |
| 0 | 1 | is_new |
| 1 | 2 | is_done |
| 2 | 4 | category ongoing |
| 3 | 8 | category hiatus |
| 4 | 16 | category canceled |
| 5 | 32 | category completed |
| 6 | 64 | is_dim |

Bits 7+ stay free (`status_flags` is already the reserved extension point per its existing comment). One-hot category encoding means a new plugin status costs one bit, not a migration, and category filtering becomes a mask test instead of a string compare.

The flags SHALL be named as a scoped enum for the compiler-side code, while the DB column stays a plain integer:

```cpp
enum class StatusFlag : unsigned {
    kNew = 1u << 0, kDone = 1u << 1, kOngoing = 1u << 2, kHiatus = 1u << 3,
    kCanceled = 1u << 4, kCompleted = 1u << 5, kDim = 1u << 6,
};
```

Conversion uses `std::to_underlying` (C++23, `<utility>`). `<bit>` (C++20) is already available in this tree — `src/plugin/wasm/wasm_imports.cpp` includes it — but its helpers (`popcount`, `has_single_bit`, `bit_width`, `countr_zero`) do not help mask composition, so this change does not add a new `<bit>` include.

**D2 — One composition helper, used by all stat writers.**
Add a file-local helper in `store_search.cpp` that builds the integer from the entity plus the incoming change:

```
flags = category_mask(status) | (done ? 2 : 0) | (is_new && !done ? 1 : 0) | (previous_flags & 64)
```

`previous_flags & 64` is what preserves the dim bit, which is owned by the toggle path alone. Rejected alternative: patch bits individually at each call site — three writers drifting apart is exactly how the current `has_update`/bit0 duplication happened.

**D3 — Status classification is a fold-and-map, no config.**
`status_category_mask(std::string_view)`: lowercase, map `ongoing→4`, `hiatus→8`, `canceled|cancelled→16`, `completed→32`, anything else → 0. Unknown values stay stored verbatim on `status` and simply carry no category bit (spec'd behavior, no error). A table-driven registry is unnecessary for four fixed values.

**D4 — Chapters-only refresh runs the same stat write.**
In `sync_saved_stats`, compute `total` from the chapters envelope before the detail branch and call `saved_stats_update(plugin, slug, /*status=*/"", total)` when the detail envelope is absent. An empty status means "leave the status alone", which the method already honors; the call advances `total_ch` and recomputes bit0 through D2. The scheduler's "chapters only" invariant holds — no extra plugin call is made. `is_new` is derived as "previously set OR grew in this call" so a flat refresh neither raises nor clears it.

**D5 — Drop `has_update` (BREAKING, deliberate), with a backfill first.**
`bit0` is computed from the same growth signal, but it is **not** already populated on every database: the live `data/library.db` has no `status_flags` column at all while 18 of 31 rows carry `has_update = 1`. A plain `DROP COLUMN` would silently erase the New state for those rows. The migration is therefore ordered, each step guarded by `pragma table_info` so it is idempotent across boots:

1. add `status_flags` (the existing additive step);
2. backfill every row from the columns that still exist, preserving bit6:
   ```sql
   UPDATE saved_manga SET status_flags =
     (CASE WHEN has_update <> 0 AND NOT (total_ch > 0 AND read_ch >= total_ch) THEN 1 ELSE 0 END)
   | (CASE WHEN total_ch > 0 AND read_ch >= total_ch THEN 2 ELSE 0 END)
   | (CASE lower(status) WHEN 'ongoing' THEN 4 WHEN 'hiatus' THEN 8
          WHEN 'canceled' THEN 16 WHEN 'cancelled' THEN 16
          WHEN 'completed' THEN 32 ELSE 0 END)
   | (status_flags & 64);
   ```
3. drop `has_update`.

Scope of the field removal: `store.hpp`, `store_internal.hpp`, the column mapping, the `has_update` assignments in `saved_row`/`saved_list`/`saved_get`/`save_manga`, the JSON field in `http_api_library.cpp` and `http_pages.cpp`, and `data-update` in `grid_body.mustache`. This intentionally breaks the repo's additive-only migration convention — an explicit decision for this change; the backfill is what keeps existing libraries intact. `ponytail:` note the `DROP COLUMN` needs SQLite ≥ 3.35.

**D6 — Dim moves onto the row.**
`store.saved_dim_update(plugin, slug, on)` sets or clears bit6 and touches nothing else; `store.saved_dim_list()` returns the `(plugin, slug)` pairs with bit6 set. `POST /api/display/dim-blur` and `GET /api/display/dim-list` rewire onto these; `kDimPrefix` and its `plugin_state` keys retire. Consequence: an unsaved manga has no row to hold the bit, so the toggle becomes a no-op there — documented in the delta and covered by a scenario. Chapter-level `host/skip/` and `host/read/` keys are untouched (different axis, per-chapter).

**D7 — Tests and golden.**
- `test_store`: one lifecycle subcase — fresh save (no bits) → growth sets bit0 → `read_ch >= total` sets bit1 and clears bit0 → each known status sets exactly its category bit (and `cancelled` folds to canceled) → unknown status sets none → dim toggle sets/clears bit6 → `saved_stats_update` and `saved_read_rollup` preserve bit6 → `saved_clear_update` clears bit0 only. Rewrite the existing `has_update` assertions to read bit0.
- `test_templates`: detail page renders each badge only when its bit is set; `inject_status_flags` writes the integer into the envelope data object (and leaves a non-object envelope alone).
- Scheduler: chapters-only `sync_saved_stats` with a grown chapter list sets `total_ch` and bit0; unchanged count leaves bit0 alone.
- Re-lock the detail-page golden last (`CPPISEKAI_UPDATE_GOLDEN=1`), since badges change the SSR.

## Risks / Trade-offs

- **Bit6 preservation.** Three writers rebuild the integer; a missed mask silently undims entries. Mitigated by putting the mask in one helper (D2) and asserting it in the lifecycle test.
- **Irreversible migration.** `DROP COLUMN` cannot be undone in place and SQLite needs ≥ 3.35 (repo builds 3.54). Accepted deliberately; `ponytail:` note on the migration naming the ceiling.
- **Behaviour loss:** dim on an unsaved manga no longer persists. Explicit consequence of storing per-manga state on the saved row.
- **API break:** consumers reading `has_update` must switch to `status_flags & 1`.
- **Golden churn:** the detail-page fixture must be regenerated; only badge markup should move.
