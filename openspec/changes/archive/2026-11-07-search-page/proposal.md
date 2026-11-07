# Proposal: search-page

## Why

`/search` is a blank stub while the plugins page and library are live. Search is the
primary entry point for new manga and the first page governed by the user ruling from
plugins-page: the user picks ONE enabled plugin, then queries it. The current ABI search
also cannot paginate (no total, no page param) and has no genre filter, which the site
experience requires.

## What Changes

- Real `/search` page: plugin picker (enabled only, logo, falls back to initial), query box
  + explicit Search button, genre chips sourced from the plugin, result count, cover grid,
  pagination above and below the grid, empty/loading/error states, URL-encoded state
  (`?plugin&q&tags&page`).
- ABI search response becomes `{"ok":true,"data":{"items":[...],"total":N}}` with `page`
  (1-based) and optional `tags` (AND-filter) params; breaking change to the one consumer.
- New optional plugin method `genres` -> `{"ok":true,"data":["Isekai",...]}`; page hides
  the genre row when the plugin does not implement it.
- mangadex: implements `genres` (from `/manga/tag`), `tags` filter, `page`, `total`.

## Capabilities

### New Capabilities
- `search-page` — layout, plugin gating, genre row, states, URL state.

### Modified Capabilities
- `mangadex-plugin` — search shape (items/total, page, tags) + `genres` method.

## Impact

- `templates/search.mustache` + route (alpine, mirrors plugins page).
- `plugins_dev/lua/mangadex/main.lua` — search rewrite + genres.
- `src/http`: none beyond existing ABI route (page talks to plugins via it).
- Tests: fixtures search shape if present; live verify against mangadex.
