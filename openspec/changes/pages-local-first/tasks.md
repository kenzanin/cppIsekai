# Tasks: pages-local-first

## 1. Reader route reads the durable page list

- [x] 1.1 In `GET /api/pages`, resolve the target release from `chapters` rows
      (`chapters_envelope_from_rows`) and keep `call_cached("chapters")` only for a row-less pair
- [x] 1.2 Serve `chapter_pages_urls()` through `pages_envelope_from_urls()` per candidate before any
      plugin call; keep `call_cached("pages")` only for a release with no rows (design D1)
- [x] 1.3 Keep the cold path's write-through (`chapter_pages_replace`) and the sibling-release walk
      unchanged; a local hit must not consult `chapter_cached` (design D2)
- [x] 1.4 `xmake test test_pages` green, including a check that the row-built list feeds
      `build_pages_response` into the same page URLs as a parse envelope

## 2. Silent revalidation

- [x] 2.1 After a locally served response, if the chapter is not fully on disk, submit one
      `plugin_pool.detach_task` that re-parses the release and calls `chapter_pages_replace` on a
      successful non-empty result (design D3)
- [x] 2.2 Throttle the task with a process-lifetime `(plugin|slug|chapter_id)` set under one mutex;
      cold-path parses schedule nothing
- [x] 2.3 Add the `ponytail:` ceiling note (once per process, upgrade path = status-aware
      invalidation) next to the throttle

## 3. Reader window and stale layer

- [x] 3.1 `reader_window`: chapters from `store.chapters_list` (one db hop with the skip flags),
      page lists from `chapter_pages_urls`; `call_cached` only when the title has no chapter rows
      (design D4)
- [x] 3.2 Drop `pages` from `stale_optin`; delete `cache_ttl_for` and use the 24h constant in
      `effective_ttl` (design D5)
- [x] 3.3 `xmake test test_window test_stale` green

## 4. Verification

- [x] 4.1 `openspec validate pages-local-first --strict` clean
- [x] 4.2 `xmake -y` zero new warnings; full `xmake test` green
- [x] 4.3 Live smoke: open a chapter of a stored title with the plugin disabled — pages render from
      rows; re-enable, open a partially cached chapter — response is local and one revalidation lands
      in the log
- [x] 4.4 Live smoke: open a never-opened chapter — the plugin is called once and rows are written
- [x] 4.5 `openspec archive pages-local-first` after all tasks tick
