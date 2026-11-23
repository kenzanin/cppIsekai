1: # Proposal: library-promote-on-save
2: 
3: ## Why
4: 
5: The flow the project markets as local-first is `search -> detail -> add -> open`, and it already has
6: the right shape: every plugin envelope lands in `api_cache` as a JSONB row (`search-cache`), and a
7: saved row is served ahead of the plugin (`manga-detail-table`). Two holes make the "library open
8: never touches the plugin" claim depend on luck:
9: 
10: - **Promotion is lazy.** `POST /api/library` stores only `name`/`cover`. The display detail
11:   (`authors`, `artists`, `tags`, `synopsis_display`) reaches `saved_manga` on the *next* detail
12:   open, when `saved_detail_response` misses and the route runs `materialize_detail`. A title saved
13:   and never reopened is never promoted.
14: - **Chapters are never promoted at all.** `/api/chapters` always goes through `call_cached`; the
15:   `chapters` rows written on every open are a flag/bookkeeping store, not a read source. So the
16:   chapters list dies with the cache row — and `cache_prune_expired` drops `api_cache` rows at
17:   `fetched_at + ttl + 7d` (24h TTL for detail/chapters, `scheduler.cpp`). After ~8 days an unmaterialized
18:   save, or any saved title's chapter list, goes back to the network.
19: 
20: The tempting fix — a new staging table, or a JSON1 column holding the raw envelope — duplicates a
21: store that already exists. `api_cache` *is* the temp store; `chapters` *is* the canonical chapter
22: list; `saved_manga` *is* the library row. Nothing needs to be invented, only wired.
23: 
24: ## What Changes
25: 
26: - **Add-to-library promotes what is already cached.** In the same db task as the save, the route
27:   reads the title's `api_cache` detail/chapters rows (the keys the preceding open minted) and runs
28:   the existing `materialize_detail` / `materialize_chapters` / `sync_saved_stats` path. No new
29:   table, no new column, no new parser, no plugin call. Idempotent; a no-op when nothing is cached.
30: - **Chapters get a row-served read path.** `GET /api/chapters` answers from `chapters` rows when they
31:   exist, building the same envelope a parse produces, flagged `cached:true`. This removes the
32:   `api_cache` prune cliff for saved titles; unsaved titles and cold misses keep the current
33:   cached-or-live behavior.
34: - **`chapters.release_at`** is stored verbatim (epoch or ISO text) so the row-built envelope is not
35:   lossy — the detail page renders `ch.release_at`. Additive migration, plus it joins the C3 seed
36:   fingerprint so a release-date-only change still refreshes.
37: - **Unsave wipes the pair.** `POST /api/library/remove` deletes the row, every pair-keyed satellite
  (`chapters`, `chapter_pages`, `reader_positions`, `update_attempts`, `title_meta`, pair-scoped
  `plugin_state` keys, the pair's `api_cache` rows) and the disk image tree, so re-add equals a fresh
  add from search. No FKs: the cascade is one explicit store op; plugin removal keeps its own GC.
