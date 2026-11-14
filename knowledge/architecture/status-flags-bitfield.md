---
type: Decision
title: "status_flags bitfield: bit0-6 layout + single compose_flags writer"
description: "saved_manga.status_flags is one INTEGER carrying bit0=is_new, bit1=is_done, bit2-5 one-hot status category (ongoing/hiatus/canceled/completed), bit6=is_dim; all writers compose through one helper that masks bit6 so the dim toggle survives every stat write."
tags: [store, library, status-flags, bitfield, single-writer, dim]
generated: { by: agent/mcp, at: "2026-11-14T10:29:06Z" }
status: stable
---

Decision recorded 2026-11-14 (change `status-flags-followup-cleanup`).

## Layout

`enum class StatusFlag : unsigned` in `src/store/store.hpp`; the DB column stays a plain `INTEGER`.

| bit | value | flag | meaning |
| --- | --- | --- | --- |
| 0 | 1 | kNew | growth observed beyond stored `total_ch` |
| 1 | 2 | kDone | `total_ch > 0 && read_ch >= total_ch` |
| 2 | 4 | kOngoing | source status folds "ongoing" |
| 3 | 8 | kHiatus | source status folds "hiatus" |
| 4 | 16 | kCanceled | source status folds "canceled"/"cancelled" |
| 5 | 32 | kCompleted | source status folds "completed" |
| 6 | 64 | kDim | user toggled dim+blur |

Bits 7-31 free. Conversion via `std::to_underlying`. No `#include <bit>`: its helpers do not help mask composition.

## Single-writer rule

The only writers are in `src/store/store_search.cpp` and every one rebuilds the integer from scratch, so composition lives in ONE file-local helper:

```cpp
unsigned compose_flags(const store_detail::SavedMangaEntity& e, const std::string& status, bool is_new) {
    const unsigned cat = status_category_mask(status.empty() ? e.status : status);
    const bool done = e.total_ch > 0 && e.read_ch >= e.total_ch;
    const unsigned new_bit = (is_new && !done) ? to_underlying(StatusFlag::kNew) : 0u;
    const unsigned dim = e.status_flags & to_underlying(StatusFlag::kDim);  // bit6 survives every stat write
    return cat | (done ? to_underlying(StatusFlag::kDone) : 0u) | new_bit | dim;
}
```

`status_category_mask` is a fold-and-map over four fixed strings; an unknown status yields 0 and the raw `status` string is still stored verbatim for display. `saved_clear_update` clears bit0 only (`&= ~kNew`).

## Why one-hot

A new plugin status costs one bit instead of a migration, and category filtering becomes a mask test instead of a string compare. The raw `status` column is NOT removed — it remains the display source for the detail chip.

## Why not more bits

Reviewed and rejected: `is_started` (`read_ch > 0`), `is_dup` (`dup_group != 0`), `has_synopsis`, `has_alt_titles`, `is_length_unknown` (`total_ch == 0`), `is_never_opened` (`last_read_at == 0`). All are derivable from another column on the same row, which is exactly the `has_update` duplication that forced a migration. Rule: a bit earns its place only when it is boolean AND not derivable from the same row.

Off-table candidates (not merged): `reader_positions.mode`/`direction` belong on that table (it is REPLACEd per page turn); `update_attempts.outcome` covers unsaved titles too, so folding it into `saved_manga` would lose coverage.

# Related Concepts
- [library-grid-stats: saved_manga stat columns + envelope-derived sync](library-grid-stats.md): The bitfield decision supersedes the earlier bit0/bit1-only status_flags contract described by library-grid-stats.
- [Per-manga dim+blur cover toggle](per-manga-dim-blur.md): Dim state is status_flags bit6; the dim toggle writes through saved_dim_update on this bitfield.
