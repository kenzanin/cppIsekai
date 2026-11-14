---
type: Fact
title: "status_flags: followup gaps left open by the detail-source-of-truth fix (c0fce4e)"
description: "Four known gaps after making detail open the single writer of saved_manga.status_flags: the auto-update scheduler skips the detail envelope so total_ch/has_update (and thus the New badge) never advance there; the flag lifecycle has no store-level test; inject_status_flags has no test; and the detail page badge bits are not locked by test_templates."
tags: [library, status-flags, badge, auto-update, followup, test-coverage]
generated: { by: agent/reasonix, at: "2026-11-14T05:49:49Z" }
status: stable
---

Recorded 2026-11-14 after c0fce4e ("library: make detail the source of truth for status_flags badges").

## G1 (functional, highest value): auto-update scheduler never sets total_ch / New badge

`src/update/scheduler.cpp` calls `refresh_title(..., /*chapters_only=*/true)`. In `src/http/api_ctx.hpp`, `chapters_only` makes `detail_env` an empty string, and `sync_saved_stats` only calls `store.saved_stats_update(...)` inside the `if (const auto d = detail::envelope_object(detail_env))` branch. Consequence: the auto-update path only runs the chapters rollup (`read_ch`); `total_ch` and `has_update` (hence `status_flags` bit0 / the New badge) are never updated by the background scheduler.

Fix shape: in the chapters-only path, still update `total_ch` from the chapters envelope (e.g. hoist the `total` computation out of the detail branch, or add a `saved_stats_update(plugin, slug, "", chs->size())` call), then the existing rollup recomputes the flags. Verify: run a scheduled refresh against a title whose chapter count grew and assert `status_flags & 1`.

## G2: store-level flag lifecycle is untested

`tests/test_store.cpp` has zero assertions on `status_flags`, so the rollup / stats_update / clear_update flag math (bit1=done, bit0=news, done suppresses news, reader touch clears bit0) is only covered indirectly by the template lock. Add assertions to the existing "stats_update / read_rollup / clear_update lifecycle" subcase: after `saved_stats_update(...,12)` -> flags has bit0; after rollup to full read -> bit1 and not bit0; after `saved_clear_update` -> bit0 cleared.

## G3: `inject_status_flags` has no test

The new helper in `api_ctx.hpp` (injects the stored bitfield into the detail envelope's `data`) is untested. Add a small unit check that a valid envelope gains `data.status_flags`, and that a malformed/absent envelope is returned unchanged.

## G4: detail-page badge bits are not locked

`tests/test_templates.cpp` locks the GRID badge (`.news`/`.done` from `data-status-flags`) but nothing asserts the detail page reads `detail.status_flags & 1` / `& 2`. Add a check on the rendered `detail.mustache` (the badge spans should gate on `detail.status_flags`, not on a client-side `hint`).

## Note

`test_templates`' grid badge test was already stale at b958e60 (it asserted the pre-bitfield `dataset.read`/`dataset.update` arithmetic and failed at HEAD); c0fce4e re-locked it to the bitfield semantics. G4 is the matching detail-page half.
