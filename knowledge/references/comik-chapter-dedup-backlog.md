---
type: reference
title: "comik chapter dedupe by number + prune of vanished chapter ids (uncommitted WIP, resume here)"
description: "Uncommitted WIP at 2026-11-27: comik chapters.lua merges per-scanlation-group duplicate rows by chapter number (Ch.N parse, not the volume digit; earliest release_at wins; number 0 stays per-id) and Store::chapters_upsert prunes chapter ids the latest parse no longer lists. Left to do: build + xmake test, live check on comik, empty-keep_ids edge for not_in, residue cleanup, commit."
tags: [backlog, comik, plugin, lua, store, dedupe, wip, resume-here]
generated: { by: agent/mcp, at: "2026-11-27T02:30:41Z" }
status: stable
---

# comik chapter dedupe + vanished-id prune — resume notes

State as of 2026-11-27 (working tree, **uncommitted**, branch `pages-local-first`).

## Already written (not yet built/tested)

### `plugins_dev/lua/comik/chapters.lua`
- comik's aggregator returns one row per scanlation group, so the same chapter
  arrives 5-10x with different ids.
- Merge key changed from chapter id to **chapter number**: first (highest-ranked)
  row wins the identity, later rows only lower `release_at` (earliest release).
- Number parse fixed: labels look like `Vol.0 Ch.30`, so match `[Cc]h[%a%.]*%s*(%d+%.?%d*)`
  first, plain digit run only as fallback — the volume digit is no longer taken as
  the chapter number.
- `number == 0` (unknown) rows are never merged, each id stays separate.
- Sort: number desc, tie-break `release_at` asc (stable across sources).

### `src/store/store_chapters.cpp`
- `chapters_upsert` now treats a parse as the **whole** chapter list: ids absent from
  the latest parse are removed (`std::erase_if` on the ephem path; `remove_all(... not_in(...))`
  inside the transaction on the persistent path). Without this the folded comik
  duplicate ids linger and double up in the reading list.
- `tests/test_store.cpp` subcase updated: `b2` vanished after re-parse, so its flags
  row must be gone (`CHECK_FALSE(... .has_value())`).

## Remaining

1. `xmake -y` + `xmake test` (expect zero warnings, store + plugin tests green).
2. Live check: comik title page → chapter list has no duplicate numbers, order sane,
   reader still opens a chapter, and the pruned rows don't break saved read progress.
3. Edge to verify in `store_chapters.cpp`: `not_in(..., keep_ids)` when `keep_ids` is
   **empty** (plugin returned zero chapters) — confirm sqlite_orm emits valid SQL
   (or guard: delete-all-for-slug instead). Same question for the ephem branch.
4. Confirm no other plugin relies on the old "upsert never deletes" behaviour
   (atsumaru/mangadex/chikari re-parse paths).
5. Clean residue before commit: `M .kit.yml`, `.kit.yml.bak-*`, `anam-adapter.log`.
6. Commit subject draft: `comik: merge chapters by number + prune vanished ids on re-parse`.

# Related Concepts
- [comik search ignored ?page -- every UI page returned the same 48 cards](../findings/comik-search-pagination-2026-11-25.md): Same plugin (comik), follow-up work after the search pagination fix.
