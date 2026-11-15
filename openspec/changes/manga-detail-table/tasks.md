# Tasks: manga-detail-table

## 1. Store: display detail on saved_manga

- [ ] 1.1 Add `authors`, `artists`, `tags`, `synopsis_display` to the saved-manga entity **and** the
      additive migration (`TEXT DEFAULT ''`), in the same commit (design D5 — a build with the
      columns against an old DB breaks every `saved_manga` read)
- [ ] 1.2 Leave `synopsis` and `alt_titles` source-only; they keep feeding `recompute_dup_groups`
      (design D4)
- [ ] 1.3 Expose the new fields on `SavedManga` and `GET /api/library`
- [ ] 1.4 Add `saved_detail_put(...)` writing the resolved display fields, skipping the write when
      they are unchanged; add `saved_detail_clear(...)` for reset
- [ ] 1.5 `xmake test test_store` green with cases for the write, the no-op write, the empty-row read
      and clustering output unchanged

## 2. Write-through at the route

- [ ] 2.1 Add `materialize_detail(store, plugin, slug, resolved_env)` next to the detail response;
      do **not** put the write inside `apply_chosen` (design D2 — the enrichment guard test asserts
      candidates never touch `saved_manga`)
- [ ] 2.2 Call it from `/api/detail` for a saved row only, and from `refresh_title` when the detail
      method ran
- [ ] 2.3 Persist the merged tag list the page renders (hidden filtered), not the raw source list
- [ ] 2.4 Confirm enrichment fetches and `chosen` saves still write `title_meta` only

## 3. Read-first path

- [ ] 3.1 Serve `/api/detail` for a saved row with stored detail from the row, without a plugin call
- [ ] 3.2 Keep the `enrichment: {fetched_at, chosen}` block and `chosen_cover` in that response so
      the picker hydrates (design D7)
- [ ] 3.3 Flag the row-served response `cached: true` (design D8)
- [ ] 3.4 Keep stale-serve and `cached:true` semantics for unsaved titles and for a saved row whose
      stored detail is still empty (cold miss resolves + materializes)
- [ ] 3.5 Disabled plugin: still serve the saved row; `POST /api/refresh` keeps answering
      `plugin disabled` (design D10)
- [ ] 3.6 Confirm the library grid card name and the detail heading read the same row value

## 4. Reset, upsert guard, UI

- [ ] 4.1 `POST /api/enrichment/reset` (`detail|cover|all`) also clears the materialized fields so
      the next open re-resolves (design D6) — keep `title_meta.fetched`
- [ ] 4.2 `POST /api/library` skips a name write that is empty or equals the slug and never
      overwrites a real stored name with one (design D9)
- [ ] 4.3 Hydrate `templates/detail.*` from the row-served response; confirm no markup change beyond
      the data source, re-lock goldens only if the rendering actually differs
- [ ] 4.4 Keep the chapter list on the existing live/stale path

## 5. Verification and close-out

- [ ] 5.1 `xmake -y` clean with `-Wall -Wextra`; `xmake test` full suite green
- [ ] 5.2 `test_http_enrich` guard case "enrichment writes never touch saved_manga / api_cache" still
      green (proves materialization stayed out of `apply_chosen`)
- [ ] 5.3 `test_saved_stats` green: synopsis/alt_titles source writes unchanged, clustering stable
- [ ] 5.4 Live check: open a saved manga's detail with the plugin unreachable and confirm the metadata
      renders from the row; press ⟳ Update and confirm the row is rewritten
- [ ] 5.5 `openspec validate manga-detail-table --strict` clean
