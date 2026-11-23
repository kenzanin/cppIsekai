# Tasks: library-promote-on-save

## 1. Promotion at save

- [x] 1.1 In `POST /api/library`'s db task, after `save_manga`, read `api_cache` for
      `stale_key(plugin, "detail"|"chapters", {"id":slug})` and run `materialize_detail` (after
      `apply_chosen`) + `materialize_chapters` + `sync_saved_stats` for the pair (design D2)
- [x] 1.2 Keep the promotion a silent no-op when a row is absent or unparsable; no plugin call, no
      error response change
- [x] 1.3 Confirm the name guard still wins: a slug/empty submitted name never overwrites a stored
      name, on both the save and the promotion write
- [x] 1.4 `xmake test test_http_library` (or nearest) green: promoted add, add-with-no-cache,
      re-add no-op, unchanged-write skip

## 2. Chapters as a read source

- [x] 2.1 Add `release_at` (`TEXT DEFAULT ''`) to `ChapterEntity` **and** the additive
      `ALTER TABLE chapters` in `store_core.cpp`, same commit (design D4)
- [x] 2.2 Thread `release_at` through `ChapterSeed`, `chapters_upsert`, and
      `chapters_fingerprint`; `materialize_chapters` reads it from the envelope
- [x] 2.3 Add a store read returning the pair's chapter rows ordered by number (design D3)
- [x] 2.4 `title_data_response("chapters")`: serve the rows when non-empty (envelope `cached:true`),
      else keep `call_cached`; materialize before serving
- [x] 2.5 `xmake test test_store` green: release_at round-trip, fingerprint sensitivity, ordered read

## 3. Verification

- [x] 3.1 Live smoke: open a new title (cold), add it, prune its `api_cache` rows by hand, disable
      the plugin, reopen the detail page — metadata + chapter list still render with the cached badge
- [x] 3.2 Re-enable the plugin and press ⟳ Update on that entry — `chapters.release_at` refreshed,
      `saved_manga` detail rewritten
- [x] 3.3 Full `xmake test` green, zero new warnings (`-Wall -Wextra`)

## 4. Unsave wipes the pair

- [x] 4.1 Extend `unsave_manga`: one txn deletes the row + chapters + chapter_pages +
      reader_positions + update_attempts + title_meta + pair-scoped host keys; raw connection
      deletes the pair's `api_cache` rows (exact id + `<slug>:...` via `json_extract`) (design D5)
- [x] 4.2 `Cache::purge_pair(plugin, slug, cover_urls)`: remove the pair's page tree
      (`<ns>/manga/<slug_dir>`) and cover files (sha16 of the stored cover URL) from disk and the
      LRU index; route captures the cover before the row goes, purges on the plugin pool
- [x] 4.3 Tests: store purge leaves neighbours + re-add fresh (`test_store`); cache purge isolation +
      re-writable pair (`test_cache`); full suite green
