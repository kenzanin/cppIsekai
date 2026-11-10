# Tasks

## 1. Store

- [x] 1.1 `src/store/store.hpp|cpp`: add `SavedManga` (`id, plugin, slug, name, cover, added_at`), table `saved_manga` with unique index on `(plugin, slug)`, created in boot storage init. Helpers: `save_manga(...)` (upsert), `unsave_manga(plugin, slug)` (delete), `saved_list()` (ordered `added_at` desc), `saved_get(plugin, slug)`.
- [x] 1.2 `tests/test_store.cpp`: round-trip test — save, re-save refreshes name/cover without duplication, unsave, list order.

## 2. API

- [x] 2.1 JSON routes: `POST /api/library` (upsert, returns `{ok, saved:true}`), `POST /api/library/remove` (delete, returns `{ok, saved:false}`), `GET /api/library` (`{ok, data:[...]}`, `added_at` desc). Store access via the existing `on_pool` gate.
- [x] 2.2 Unit-shape coverage if cheap (position-style) or manual curl checks documented in the change notes; no plugin calls involved.

## 3. Detail page toggle

- [x] 3.1 `templates/detail.mustache`: "Add to library" / "In library" toggle beside the `⟳ Update` button; `boot()` fires a membership fetch (mirror the `loadPluginMeta()` pattern: `GET /api/library`, find by plugin+slug) before/alongside `load()`; POSTs to `/api/library` / `/api/library/remove` and flips label. Failure surfaces in the existing `err` line, no blocking banner.

## 4. Library grid

- [x] 4.1 `home_context` in `src/http/http_pages.cpp`: if `saved_manga` is non-empty, build the saved list (`added_at` desc) as the card source and render the saved-mode partial for both `/` and `/library/grid`; `?plugin`/`?q`/`?page` are ignored in saved mode (pager hidden). Otherwise keep the existing placeholder (`title`) grid — except when `saved_manga` is empty AND the user explicitly opened the Library? No: empty saved => empty state, not search cache (spec scenario).
- [x] 4.2 `templates/grid_body.mustache`: saved-mode markup next to the placeholder markup — saved cards render a real cover via `/cache/img?url=<cover>&cover=1&plugin=<p>&slug=<s>` (empty cover falls back to the `hsl(hue)` div, unifying the card frame), `@error` hides the broken img leaving the fallback; name + plugin caption; links to `/view/manga/<plugin>/<slug>`; empty state message verbatim: "No saved manga — open a detail page and click Add to library".
- [x] 4.3 `templates/home.mustache`: pass-through unchanged (grid container markup stays); `grid_body.mustache` branches on the saved-mode flag set by `home_context`.

## 5. Verification

- [x] 5.1 `ctest` green (`test_store` extended); `openspec validate --change library-save` clean (note: `openspec validate` accepts `--changes` for all; per-change flag may not exist — fall back to `--changes` and grep the line).
- [x] 5.2 Live smoke (Chrome via blueprint): save from detail → card appears on home with correct name/cover; unsave → disappears; empty state when `saved_manga` is empty; detail toggle state correct on reload; saved view ignores `?page=`; broken cover falls back to hue div.
