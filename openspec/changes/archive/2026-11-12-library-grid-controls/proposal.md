## Why

The Library grid shows bare cover cards: no way to filter by status, no way to sort, no read-progress hint, no duplicate visibility. Users asked for a richer grid: total-titles counter in the sidebar that resets filters, per-card read progress + Done badge, status/plugin/New overlays on covers, plugin-name click-to-filter, and sort by title / progress / recent read / recent update plus filters duplicate / new / done / complete. The grid currently paginates server-side 12/page, which is incompatible with cheap client-side filtering; the saved set is small enough to render client-side.

## What Changes

- Sidebar: total-titles card at the very top; clicking it resets all filters and shows every manga.
- Grid (saved mode): renders all saved entries; client-side (Alpine) search/filter/sort/pagination.
- Card: `read_ch/total_ch` progress line (when stats present); `Done` badge when read >= total and total > 0; `New` badge overlay top-left when `has_update=1`; `status` chip overlay top-left (else New takes precedence? no — New left-top, status left-top stacks? resolved: New badge top-left, status chip below it or top-right? -> see design D3); plugin name bottom-left overlay; click plugin name filters grid to that plugin.
- Plugin list in sidebar: clicking a plugin name filters the grid to that plugin (replaces plain navigation? no — keeps hx navigation AND adds filter toggle? resolved: it filters client-side within saved grid; the existing hx-get stays for deep link).
- Sort dropdown: title | progress | recent read | recent update. Filter chips: duplicate, new, done, complete (+ reset-all button).
- Empty-filter state: "no manga matches" + reset button.
- Stat fields come from `library-grid-stats`; controls degrade gracefully when absent (no progress line, filters no-op).

No store schema changes in this change (columns land in `library-grid-stats`).

## Capabilities

### New Capabilities
None (extends library-save UI).

### Modified Capabilities
- `library-save`: saved grid requirements change from server-paginated 12/page to client-side filter/sort/paginate; sidebar gains total-reset card.

## Impact

- `templates/home.mustache`, `templates/grid_body.mustache` (+ new `grid_script.mustache` if needed), `src/http/http_pages.cpp` (render all saved rows + data-* attrs), golden test regen.
- Depends on `library-grid-stats` for progress/Done/New/update fields (renders placeholders absent).
- Dedup filter chip renders only when duplicate grouping landed (`library-dedup`); otherwise hidden.
