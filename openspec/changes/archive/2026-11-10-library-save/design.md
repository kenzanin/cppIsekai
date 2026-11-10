# Design

## Context
See proposal.md. Constraints:

- Home page (`/`) is the Library; htmx tail `/library/grid` swaps `grid_body.mustache`; card markup is a colored `aspect-[3/4]` placeholder today — there is no cover img path wired into the grid.
- Detail page already fetches membership-style JSON in `boot()` (`loadPluginMeta()` → `/api/plugins`, finds by `.key`); the toggle mirrors it one-for-one.
- `home_context` lives in `src/http/http_pages.cpp` and renders for both `/` and `/library/grid`; pagination/filtering is driven by `?plugin=`/`?q=`/`?page=` and consumed by the mustache pager.
- Store is sqlite_orm; `create_storage()` runs at boot; `test_store` pattern exists for round-trip assertions.

## Goals / Non-Goals
**Goals:** saved state owned by the store; saved mode replaces the grid render when any saved entries exist; detail toggle correct on first load; empty state for fresh installs.
**Non-Goals:** status fields (reading/done/dropped), folders, cover editing, pagination/filtering/search of the saved list, back-fill from the search cache — follow-ups.

## Decisions
- **D1 — New `saved_manga` table, `title` untouched.** Columns: `id` pk, `plugin`, `slug`, `name`, `cover`, `added_at` (unix seconds). Unique index on `(plugin, slug)` — upsert via insert-or-update. Why: saved rows are denormalized (name/cover carried) so the grid never calls plugins at render; `title` lacks slug/cover and FK-coupling saved rows to the plugin registration lifetime buys nothing.
- **D2 — Un-save = hard delete**, not a soft flag. Why: no history requirements today; simplest correct model.
- **D3 — Saved mode on the existing grid container.** `home_context(pages)` short-circuits to the saved list when `saved_manga` is non-empty: `/` and `/library/grid` render the same saved partial (user-scoped saved data ignores `plugin`/`q`/`page` — the pager markup is skipped in saved mode rather than faking paging). Empty saved list keeps the existing placeholder-title grid untouched? **No — spec scenario says empty state, not search cache.** So: empty saved → mustache empty state; non-empty → saved cards.
- **D4 — Toggle state hydrated client-side** via `GET /api/library` membership (plugin+slug) in `boot()`, mirroring `loadPluginMeta()`. Why: detail is server-rendered but membership is DB state; piggybacking on `load()` would make save state latency-sensitive; one tiny JSON fetch fits Alpine.
- **D5 — `POST /api/library` and `POST /api/library/remove`** (matches all-writes-are-POST convention; Crow DELETE with JSON bodies unused in the codebase).
- **D6 — Saved cards are cover-real.** `/cache/img?url=<cover>&cover=1&plugin=<p>&slug=<s>` + `onerror` hides the img leaving a hue div fallback (same pattern spirit as the detail cover). Cover URL empty → hue div renders as-is. Ordering `added_at` desc.
- **D7 — The empty-state message is the one from the spec**, verbatim: "No saved manga — open a detail page and click Add to library". (Hyphen-minus, ASCII-ish except the em-dash; keep UTF-8.)

## Risks / Trade-offs
- [saved_manga rows keep stale name/cover after upstream metadata changes] → acceptable; re-save refreshes via upsert; toggle re-hits `GET /api/library` next boot.
- [Saved view bypasses plugin filter params silently] → saved view ignores `?plugin`/`?q`/`?page`; documented in the spec scenario; the placeholder pager is not rendered in saved mode.
- [Boot migration for a new table] → `create_storage()` creates it at next boot; fresh table, no backfill.
- [Cover 404s for dead mangadex uploads] → `@error` fallback to the hue div; consistent with detail page.

## Migration Plan
Additive. Rollback = revert; dropping `saved_manga` loses saved entries.
