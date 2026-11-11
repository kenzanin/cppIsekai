## Why

The Library holds the same manga across sites (or the same series re-uploaded with re-translated titles and different synopses). Users want a duplicate filter that finds entries that are the same work, not just same-titled saves. The goIsekai reference used SM25 token overlap + FTS5/BM25; our vendored libsql amalgamation already ships FTS5 (per `decisions/fts5-search-dedup` probe notes) and both `title_meta` (alt titles from enrichment) and the plugin `detail` envelope carry synopsis + alt titles — all usable as duplicate signal.

## What Changes

- Persist on the saved row (or derive on demand): `synopsis` and `alt_titles` from the plugin source detail envelope (source plugin, NOT enrichment — per user directive). Stored only for saved entries.
- FTS5 virtual table `library_fts(name, alt_titles, synopsis)` covering saved entries; kept in sync in store methods (`save_manga`/unsave/`saved_stats_update`) via triggers or explicit delete+insert in the same transaction.
- Duplicate detection: for each saved entry, FTS5 MATCH tokenized title/alt-titles to find candidates; score by BM25; confirm pair by token-overlap (Dice/Jaccard) over normalized title tokens + synopsis tokens; pairs with similarity >= threshold (default 0.6, tune) form connected components → `dup_group` integer assigned per saved row (`dup_group = 0` = unique).
- Filter chip "duplicate" in the grid shows only rows with `dup_group != 0`, clustered so duplicates render adjacently (grid sort by `dup_group` desc, then name, when the chip is active).
- Refresh triggers re-clustering (cheap: N saved entries).
- Synopsis/alt-title already in fetch detail wizard responses — no UI change required there for this change (displaying them in wizard is a later change, noted but not implemented).

No new dependencies. No plugin ABI change.

## Capabilities

### New Capabilities

### Modified Capabilities
- `library-save`: saved entries may carry `synopsis`, `alt_titles`, `dup_group`; FTS5-backed duplicate grouping; grid "duplicate" filter clusters duplicates adjacently.

## Impact

- `src/store/*` (new FTS5 virtual table via raw connection, store sync hooks), `http_pages.cpp` (grid rows emit `data-dup`), dedup recompute job on refresh/save, `library-grid-controls` chip becomes live.
- Read-parallel DB is NOT introduced here; dedup scan uses a read-only one-shot raw connection on demand (option 3 from discussion) — acceptable because dedup runs on user action / refresh, not on render.
