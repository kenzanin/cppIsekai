# Tasks

## 1. Store

- [ ] 1.1 `src/store/store.hpp|cpp`: add `SavedManga` (`id, plugin, slug, name, cover, added_at`), table `saved_manga` with unique index on `(plugin, slug)`, created in boot storage init. Helpers: `save_manga(...)` (upsert), `unsave_manga(plugin, slug)` (delete), `saved_list()` (ordered `added_at` desc), `saved_get(plugin, slug)`.
- [ ] 1.2 `tests/test_store.cpp`: round-trip test — save, re-save refreshes name/cover without duplication, unsave, list order.

## 2. API

- [ ] 2.1 JSON routes: `POST /api/library` (upsert, returns `{ok, saved:true}`), `POST /api/library/remove` (delete, returns `{ok, saved:false}`), `GET /api/library` (`{ok, data:[...]}`, `added_at` desc). Store access via the existing `on_pool` gate.
- [ ] 2.2 Unit-shape coverage if cheap (position-style) or manual curl checks documented in the change notes; no plugin calls involved.

## 3. Detail page toggle

- [ ] 3.1 `templates/detail.mustache`: "Add to library" / "In library" toggle beside the `⟳ Update` button; state fetched from `GET /api/library` (membership by plugin+slug) on boot; POSTs to save/remove and flips label. Failure surfaces in the existing `err` line, no blocking banner.

## 4. Library grid

- [ ] 4.1 Home/grid (`templates/home.mustache`, `templates/grid_body.mustache`, `home_context` in `src/http/http_pages.cpp`): when `saved_manga` is non-empty, render saved entries as cover cards linking to `/view/manga/<plugin>/<slug>` (ordered `added_at` desc); otherwise render the empty state ("No saved manga — open a detail page and click Add to library"). Search-cache pagination/filtering untouched when rendering saved entries (no pagination for the saved view in this change).
- [ ] 4.2 Card cover uses `/cache/img?...&cover=1` with `@error` degradation consistent with the detail page.

## 5. Verification

- [ ] 5.1 `ctest` green (`test_store` extended), `openspec validate --change library-save` clean.
- [ ] 5.2 Live smoke (Chrome via blueprint): save from detail → card appears on home with correct name/cover; unsave → disappears; empty state on fresh/empty DB; detail toggle state correct on reload.
