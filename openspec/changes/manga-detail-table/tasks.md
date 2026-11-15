# Tasks: manga-detail-table

## 1. Store: display detail on saved_manga

- [ ] 1.1 Add `authors`, `artists`, `tags` to the saved-manga entity and the additive migration
      (TEXT DEFAULT ''), joining the existing `name`, `cover`, `synopsis`, `status`
- [ ] 1.2 Expose the fields on `SavedManga` and `GET /api/library` (values only, no re-derivation)
- [ ] 1.3 Add `saved_detail_put(...)` writing the resolved display fields and skipping the write when
      they are unchanged
- [ ] 1.4 `xmake test test_store` green with cases for the write, the no-op write and the empty-row read

## 2. Write-through on resolve

- [ ] 2.1 In the detail API, persist the fields produced by `apply_chosen` (post `ensure_name`) for a
      saved row only; leave unsaved titles untouched
- [ ] 2.2 Persist `tags` as the merged, hidden-filtered list the page renders (not the raw source list)
- [ ] 2.3 Confirm enrichment fetches and `chosen` saves still write `title_meta` only
- [ ] 2.4 Clear the materialized fields on `Reset detail` / `Reset cover` so the next resolve repopulates

## 3. Read-first path

- [ ] 3.1 Serve `/api/detail` for a saved row with stored detail from the row, without a plugin call
- [ ] 3.2 Keep the stale-serve and `cached:true` semantics for unsaved titles and for a saved row whose
      stored detail is empty
- [ ] 3.3 `POST /api/refresh` calls the plugin, validates both envelopes and rewrites the stored detail
- [ ] 3.4 Confirm the library grid card name and the detail heading read the same row value

## 4. Detail page

- [ ] 4.1 Hydrate `templates/detail.*` from the stored detail when present; no new fetch shape
- [ ] 4.2 Keep the chapter list on the existing live/stale path
- [ ] 4.3 Re-lock goldens only if the rendered markup actually changes; review that diff

## 5. Verification and close-out

- [ ] 5.1 `xmake -y` clean with `-Wall -Wextra`; `xmake test` full suite green
- [ ] 5.2 Live check: open a saved manga's detail page with the plugin unreachable and confirm the
      metadata renders from the row
- [ ] 5.3 `openspec validate manga-detail-table --strict` clean
