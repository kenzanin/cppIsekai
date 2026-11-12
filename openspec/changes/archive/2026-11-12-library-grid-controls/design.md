## Context

Saved grid renders from `store.saved_list()` in `home_context` (`http_pages.cpp`) → `grid_body.mustache` cards. Today: server-paginated 12/page, `hx-get /library/grid`, sidebar shows `{{total}} titles` and `{{#plugins}}` rows that navigate (`?plugin=`) server-side. Grid is small (user library, tens–hundreds).

Stat fields (`status/total_ch/read_ch/has_update/last_read_at/updated_at`) land in `library-grid-stats`; duplicate grouping in `library-dedup`. This change must degrade gracefully when they are absent (empty/zero values → no badge, filter matches everything / nothing? see D5).

## Goals / Non-Goals

**Goals:** one grid, client-side filter/sort/paginate; sidebar reset card; overlay badges; plugin-name filter.

**Non-Goals:** store schema, FTS5, dedup clustering, read-parallel DB (option 1, deferred). Server-side filtering.

## Decisions

**D1 — Client-side everything.** Render all saved rows in one grid_body with `data-*` attributes (`data-name`, `data-plugin`, `data-status`, `data-total`, `data-read`, `data-update`, `data-added`, `data-lastread`); Alpine `x-data` owns `q`, `plugin`, `filter[]`, `sort`, `page`. hx-get stays ONLY as the initial page load / `hx-push-url` deep-link (`?plugin=` preselects the client filter). Pagination client-side, same 12/page + existing pager style. Rationale: small set, snappy, no round trips, matches user's "simple only" directive.

**D2 — Sidebar total card = reset.** New top block `{{saved_total_all}} titles` inside `<aside>`, click → clear `q/plugin/filter/sort` to defaults and `page=1`. This is the canonical escape hatch; an in-grid "Reset filters" button next to the sort dropdown mirrors it.

**D3 — Badge layout (no overlap).** Cover overlays: top-left = `New` (red) when `has_update=1`, else `Done` (green) when read>=total>0; status chip (ongoing/hiatus/complete/canceled) sits directly UNDER the New/Done badge on the same left edge; plugin name bottom-left (bg-black/70). Status colors: ongoing=blue, hiatus=amber, complete=green, canceled=gray (from prior `library-page` OKF decision).

**D4 — Plugin name click filters.** The plugin sidebar rows keep their existing server-side `hx-get` navigation BUT the in-grid plugin overlay (and a click on the plugin name in the sidebar when grid is client-side) sets the client `plugin` filter instead of navigating, when the grid is in saved mode. Deep-link `?plugin=<id>` pre-selects the client filter on load. Sidebar row active state reflects client filter.

**D5 — Graceful degradation & filter semantics.** When stat fields absent (pre-`library-grid-stats`): progress line hidden, Done/New badges hidden, status chip hidden, "new/done/complete" filters match nothing? → hidden instead (filters render only if any loaded row carries the field). "duplicate" filter renders only when `dup_group` present on ≥1 card (post-`library-dedup`). Reset always available.

**D6 — Sort keys.** `title` (localeCompare on displayed name), `progress` (read/total ratio, unread 0, done last? -> desc by ratio), `recent read` (last_read_at desc, never-read last), `recent update` (updated_at desc). Stable tiebreak by name.

**D7 — URL state.** Filter/sort/q/page NOT serialized to URL (client-only, per user's simple-only directive; deep-link `?plugin=` only).

## Risks / Trade-offs

- Client-side means large libraries (thousands) get heavy DOM; accepted for single-user local-first.
- Golden tests: `grid_body.mustache` changes → regen goldens (`CPPISEKAI_UPDATE_GOLDEN=1`).
- No duplicate/new/done data until dependent changes land — cards render without badges; keep the markup unconditional but badge visibility data-driven.
