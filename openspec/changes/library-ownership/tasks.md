# Tasks: library-ownership

## 1. Ephemeral browse store

- [x] 1.1 `StoreImpl` gains `Ephemeral`: pair -> {chapters (id/number/title/url/release_at/flags/
      read_max/pages), reader position, title_meta, per-pair host fields}; pair-count LRU bound
      (design D1); plain map, no lock (single db worker)
- [x] 1.1a Routing surface (graph-verified, callers unchanged): chapters_upsert (materialize_chapters), chapters_list (chapters_row_response), chapter_pages_replace (register_content), chapter_pages_count (chapter_read_max_bump, register_state), chapter_flags_apply + chapter_read_max_bump (register_state, register_reader), chapter_flags_list (reader_window, apply_read_rollup, register_state), position_set (register_reader), position_get (register_reader, register_pages), position_last (register_state), title_meta_* (chosen_title, chosen_cover, apply_chosen, enrichment_source_response, saved_detail_response, register_enrich)
- [x] 1.2 `saved_exists(plugin, slug)` predicate + dispatch in every pair-keyed store method
      (chapters_upsert/list, chapter_pages_replace, chapter_flags_apply/get/list,
      chapter_read_max_bump/list, chapter_pages_count, position_set/get/last, title_meta_*) —
      signatures unchanged (design D2)
- [x] 1.3 `api_cache`, `saved_*` and the plugin registry stay on their current path (design D9)
- [x] 1.4 `xmake test test_store`: unsaved pair writes nothing to SQLite yet serves the reader;
      saved pair unchanged

## 2. Promote on save

- [x] 2.1 Promotion moves the ephemeral pair into SQLite in one transaction (flags + read_max +
      pages + position + title_meta + host fields), then erases the memory entry; it reads the map
      **directly**, not through the routed readers (the new saved row flips routing) (design D3)
- [x] 2.2 Keep the existing `materialize_detail` / `sync_saved_stats` path from the cached
      envelopes; a save with nothing cached and nothing browsed stays a plain row insert
- [x] 2.3 Test: read marks produced before saving land in the library (read 5, save, read_ch = 5)
- [x] 2.4 Test: unsave wipes the promoted rows and leaves no memory entry behind

## 3. Soft orphan retention

- [x] 3.1 `plugin_prune_absent` re-scoped: `title`, `plugin_state`, `api_cache`, plugin row only;
      never saved_manga/chapters/chapter_pages/reader_positions/title_meta/update_attempts (design D6)
- [x] 3.2 `Store::plugin_upstream(id) -> {live, offline, orphan}` derived from the registry
      (design D5); expose per-row `state` in `GET /api/library`
- [x] 3.3 Gate on `live`: stale layer (no call for orphan), `refresh_title` (`plugin removed`), and
      the scheduler candidate query (replace the COALESCE-enabled fallback with an EXISTS) (design D7)
- [x] 3.4 Grid: orphan badge + orphan filter chip (reuse the existing filter mechanism)
- [x] 3.5 `POST /api/library/purge-orphans`: run the existing wipe path for every orphan pair,
      rows + api_cache + disk images (design D8)
- [x] 3.6 Tests: folder-gone scan keeps library rows and marks orphan; reinstall revives; purge
      removes only orphans; scheduler picks no orphan

## 4. Legacy cleanup

- [x] 4.1 `saved_manga` gains `chapters_fp`, `novel`, `detail_url`; boot migration copies the
      `plugin_state` values once and deletes those keys (design D10)
- [x] 4.2 Drop the dead `title` table and its `store.search`/`count` paths + the search-page seed
      branch; drop the unused `host/pos` key constant. Callers are only `http_pages.cpp:53/56`
      (`home_context`) plus the `test_store` title-seed assertions (~10), which go with it
      (graph-verified: no other caller).
- [x] 4.3 Boot migration: delete leftover `host/skip|read|total` shadows if any remain (already
      retired) and log counts

## 5. Verification

- [x] 5.1 `xmake test` green, zero new warnings (`-Wall -Wextra`)
- [x] 5.2 Live: browse a title unsaved (opens + reads), restart the daemon, confirm it is clean;
      the library is untouched
- [x] 5.3 Live: save a browsed title with 5 read chapters, verify read_ch and the per-chapter marks
- [x] 5.4 Live: move a plugin folder aside, restart, verify its titles still open (orphan, cached
      badge) and no refresh is attempted; move it back, verify live again
- [x] 5.5 README + OKF sync (ownership model, orphan semantics, ephemeral store)
