## 1. Backend render (grid_body data)

- [x] 1.1 `src/http/http_pages.cpp`: in saved mode render ALL saved rows (drop `saved_off` slice + `saved_pages` math) as `data-*` attrs on each card (`name, plugin, status, total, read, update, added, lastread, dup` when present); keep `saved_total` for the sidebar count.
- [x] 1.2 `templates/grid_body.mustache`: wrap `<ul>` in Alpine `x-data` (new `grid_script.mustache` inlined at bottom or inline x-data) with state `{q, plugin, status, filters: [], sort: 'added', page: 1}`; `visible()` getter applies q/plugin/filters + sort + paginate(12); `pageCount()`; pager markup mirroring existing style.
- [x] 1.3 Empty-filter state block + reset button when `visible().length === 0`.
- [x] 1.4 Card templates: progress line (`read/total`, x-show total>0), badges (`New` → `has_update`, else `Done` → read>=total>0), status chip, plugin overlay button (bottom-left, `@click.prevent` set plugin filter).
- [x] 1.5 Sort dropdown + filter chips row (duplicate chip hidden unless any row has `data-dup`) + reset button.

## 2. Sidebar

- [x] 2.1 `templates/home.mustache`: replace `{{total}} titles` / `more later` footer with top total-titles card (saved count) that resets client filters (dispatch CustomEvent or shared Alpine store); plugin rows get `:class` active binding via the same client state (keep hx-get deep-link for non-JS).
- [x] 2.2 `?plugin=` deep-link preselects the client filter on load (`x-init` reads `plugin` from a `data-` attr or query string).

## 3. Verification

- [x] 3.1 `ctest --test-dir build -R templates --output-on-failure`; regenerate goldens if grid_body/home changed.
- [x] 3.2 Manual smoke on `:8090` with saved library: filter/sort/reset/badges render; verify no console errors.
