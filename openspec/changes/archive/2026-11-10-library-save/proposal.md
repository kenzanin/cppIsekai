# Proposal

## Why
The home page *is* the Library (saved/watched manga), but nothing today writes into it: the store's `title` table is a search-cache seed, not a user's saved list. The user wants a persistent "add to library" control on the detail page whose additions appear on the home/Library grid. There is no saved-manga concept in the store yet.

## Current state (2026-11-10)
Detail page (`templates/detail.mustache`) is Alpine (mixed directives; one `x-text` footer as of this rewrite) and already fetches several endpoints from a single `load()`: `/api/detail`, `/api/chapters`, `/api/position/<plugin>/<slug>` (continue hint), plus `loadRelated()` (`/api/related/...`) and `loadPluginMeta()` (`/api/plugins`, membership pattern by `key`) fired from `boot()`. The toggle reuses that exact membership pattern — no new plugin calls.

The home grid is not cover-aware today: `home_context` in `src/http/http_pages.cpp` emits `{name, plugin, hue}` cards (`hue = id*47 % 360`), and `templates/grid_body.mustache` renders a colored `aspect-[3/4]` div + name + plugin, wrapped in Prev/Next nav driven by `?plugin`/`?q`/`?page`; the tail swaps via `/library/grid`. Saved cards must out-rank the placeholder grid but cannot reuse its hsl-div card markup.

Store today: sqlite_orm, one table per entity (`plugin`, `title`, `reader_positions`), created in `create_storage()` at boot; `test_store` covers round-trips. `host/` keys exist only in `plugin_state` (skip/position) — none of this is in play. `title` carries no slug/cover and stays untouched.

## What Changes
- Store: new table `saved_manga` `(plugin, slug, name, cover, added_at)` (unique on `(plugin, slug)`); upsert on save, delete on unsave, ordered list query (`added_at` desc) for the grid.
- Routes:
  - `POST /api/library` `{plugin, slug, name, cover}` → upsert → `{ok:true, saved:true}`
  - `POST /api/library/remove` `{plugin, slug}` → delete → `{ok:true, saved:false}`
  - `GET /api/library` → JSON list of saved entries (for the grid / toggle state)
- Detail page: "Add to library" / "In library" toggle beside the `⟳ Update` button; `boot()` membership check via `GET /api/library`; POSTs save/remove and flips label. Errors surface on the existing `err` line.
- Home/Library grid: when `saved_manga` is non-empty, render saved entries as cover cards (real thumbnail via `/cache/img?...&cover=1`, `@error` fallback) linking to `/view/manga/<plugin>/<slug>`; empty library renders the empty state instead of the search-cache grid. Saved view is NOT paginated in this change; plugin/search filters do not apply to it.
- Library listing reads `saved_manga`, not the `title` search-cache table.

## Capabilities
### New Capabilities
- `library-save`: save/unsave contract and the saved-manga listing backing the Library grid

### Modified Capabilities
<!-- none: detail page gains a button; home grid gains a saved-mode renderer -->

## Impact
- `src/store/store.hpp|cpp` (schema + helpers), `src/http/http_api.cpp` (JSON routes), `src/http/http_pages.cpp` + `templates/home.mustache` / `templates/grid_body.mustache` (saved-mode grid), `templates/detail.mustache` (toggle), `tests/test_store.cpp`.
- No plugin ABI change; no `openspec` main-spec touch.
- Rollback = revert; dropping `saved_manga` loses the saved list (documented, acceptable).
