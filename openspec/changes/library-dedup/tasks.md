## 1. Store signal columns

- [x] 1.1 `store_internal.hpp`: `SavedMangaEntity` += `synopsis TEXT`, `alt_titles TEXT` (JSON array), `dup_group INTEGER DEFAULT 0`; additive ALTER in boot probe (ponytail).
- [x] 1.2 Sync `synopsis`/`alt_titles` from plugin detail envelope on save, refresh, detail open (source plugin only); `alt_titles` stored as JSON text verbatim.
- [x] 1.3 FTS5 probe at boot: `CREATE VIRTUAL TABLE IF NOT EXISTS library_fts(name, alt_titles, synopsis)`; on failure warn + disable dedup (dup_group stays 0).
- [x] 1.4 Store hooks `save_manga`/`unsave_manga`/`saved_stats_update`: delete+insert `library_fts` row for `(plugin, slug)` in the same transaction.

## 2. Clustering

- [x] 2.1 Tokeniser (store_search.cpp): unicode lowercase, strip punctuation/diacritics (NFKD + mark strip), collapse ws; return multiset.
- [x] 2.2 `recompute_dup_groups()`: read-only raw connection scan; candidates via FTS5 `MATCH` per row (bm25 top 20); similarity = max(title_dice, 0.5*title_dice+0.5*synopsis_dice) ≥ 0.6; union-find components; write `dup_group` back (single txn, db_pool).
- [x] 2.3 Call sites: after save/unsave/refresh/detail-open (async ok — on_pool fire-and-forget like covers).

## 3. Grid feed

- [x] 3.1 `http_pages.cpp` saved rows: emit `data-dup=<dup_group>` (and synopsis/alt_titles NOT rendered into HTML — data attrs only where needed; dup only).
- [ ] 3.2 `library-grid-controls`: duplicate chip becomes visible (any row `data-dup != 0`) and auto-engages `dup_group` sort.

## 4. Verification

- [x] 4.1 Store test: save two entries with same normalized title/alt tokens (fixtures) → same dup_group; unrelated pair → 0.
- [ ] 4.2 Manual smoke on `:8090`: chip clusters duplicates adjacently.
