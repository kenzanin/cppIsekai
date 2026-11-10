# Proposal

## Why

The home page *is* the Library (saved/watched manga), but nothing today writes into it: the store's `title` table is a search-cache seed, not a user's saved list. The user wants a persistent "add to library" control on the detail page whose additions appear on the home/Library grid. There is no saved-manga concept in the store yet.

## What Changes

- Store: new table `saved_manga` `(plugin, slug, name, cover, added_at)` (unique on `(plugin, slug)`); upsert on save, delete on unsave, ordered list query (`added_at` desc) for the grid.
- Routes (`src/http/http_api.cpp`):
  - `POST /api/library` `{plugin, slug, name, cover}` → upsert → `{ok:true, saved:true}`
  - `DELETE /api/library` `{plugin, slug}` (or POST `/api/library/remove`) → delete → `{ok:true, saved:false}`
  - `GET /api/library` → JSON list of saved entries (for the grid / toggle state)
- Detail page (`templates/detail.mustache`): "Add to library" / "In library" toggle button beside the `⟳ Update` button; state from `GET /api/library` membership check (or a `saved` field on the detail response).
- Home/Library grid (`templates/home.mustache` / `grid_body.mustache`): saved entries appear as cover cards; each card links to `/view/manga/<plugin>/<slug>`. Empty state message when nothing is saved.
- Library listing reads `saved_manga`, not the `title` search-cache table.

## Capabilities

### New Capabilities

- `library-save`: the save/unsave contract and the saved-manga listing backing the Library grid

### Modified Capabilities

<!-- none: detail page gains a button (detail-enrichment / manga-detail-page surfaces the button; this change owns the toggle behaviour and the grid) -->

## Impact

- `src/store/store.hpp`, `src/store/store.cpp` (schema + helpers + saved query)
- `src/http/http_api.cpp` (3 JSON routes), `templates/detail.mustache` (toggle), `templates/home.mustache` / `templates/grid_body.mustache` (saved section)
- Depends on `detail-enrichment` for the button placement and `name`/`cover` payload reuse; can be implemented in parallel if the button is wired to this change's endpoint alone
- No dependency changes; one additive schema migration (new table, created at boot per existing `create_storage` pattern)
