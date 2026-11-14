---
type: Fact
title: "status-flags apply: backfill must precede DROP COLUMN has_update (18/31 rows at risk)"
description: "Applying status-flags-followup-cleanup closed G1-G5 and exposed that the has_update drop must be preceded by a status_flags backfill: the live DB had 18/31 rows with has_update=1 and no status_flags column, so dropping first would have erased the New badge for those rows."
tags: [status-flags, migration, has-update, data-loss, sqlite, library]
generated: { by: agent/mcp, at: "2026-11-14T10:18:51Z" }
status: stable
---

Recorded 2026-11-14 while applying change `status-flags-followup-cleanup`.

## G1-G5 closed

- G1 scheduler New badge: `sync_saved_stats` now computes `total` from the chapters envelope before the detail branch and calls `saved_stats_update(plugin, slug, "", total)` when the detail envelope is empty, so the chapters-only auto-update path advances `total_ch` and bit0 without a `detail` call.
- G2 store lifecycle + G4 detail-page badges + `inject_status_flags` are covered by tests; `tests/test_saved_stats.cpp` was added as its own target.
- G5 Complete -> bit5, plus the whole one-hot category set and dim bit6.

## The trap: migration order is load-bearing

Evidence from the live `data/library.db` before the change (31 saved rows):

```
status_flags column present?     0   <- column did not exist yet
has_update = 1                  18
rows already done (read>=total)  2
dup_group <> 0                   0
```

The plan first claimed "no backfill needed, bit0 already mirrors has_update". That was WRONG: with no `status_flags` column, a plain `DROP COLUMN has_update` would have erased the New state of 18 of 31 rows. The shipped migration is ordered, each step guarded by `pragma table_info` so it is idempotent across boots:

1. add `status_flags` if absent;
2. backfill every row from `has_update` / `total_ch` / `read_ch` / `status` while preserving `status_flags & 64`;
3. `ALTER TABLE saved_manga DROP COLUMN has_update` (needs SQLite >= 3.35; repo builds 3.54).

General rule for this repo: before dropping a column whose value is claimed to be "duplicated" by a bit, verify the bit column actually exists and is populated in the live DB. `pragma table_info` on `data/library.db` is the cheap check.

## API consequence

`has_update` is gone from `GET /api/library` and from the grid's `data-update`; consumers must read `status_flags & 1`. Dim on a manga that is not saved is now a no-op, because the bit lives on the saved row and `host/dim/...` plugin_state keys were retired.

## Apply state at time of writing

Applied on branch `status-flags-followup-cleanup` (16 modified files plus untracked `tests/test_saved_stats.cpp`), not yet committed or merged to `main` at record time. Verify with `git status` before assuming it landed.

# Related Concepts
- [status_flags bitfield: bit0-6 layout + single compose_flags writer](../architecture/status-flags-bitfield.md): The apply finding is the migration consequence of the bitfield decision: dropping has_update required a backfill into status_flags first.
