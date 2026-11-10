# Design

## Context

See proposal.md for motivation. Constraints:

- Home page (`/`) is the Library: `home.mustache` + htmx partials (`/library/grid` → `grid_body.mustache`), data from the `title` store table (search-cache seed) via `home_context()` in `src/http/http_pages.cpp`.
- Store is sqlite_orm with one table per entity (`plugin`, `title`, `reader_positions`), created in `create_storage()` at boot; `test_store` covers store behavior.
- Detail page currently has `⟳ Update` (stale-serve refresh) and links into the reader; the toggle lives beside it.
- No auth model exists; single-user local daemon.

## Goals / Non-Goals

**Goals:**
- Saved-manga state owned by the store; Library grid renders saved entries
- Detail page toggle with correct state on first load
- Empty state for a fresh install

**Non-Goals:**
- Status fields (reading/done/dropped), folders, covers editing — follow-up change
- Plugin-provided "library" capability (no such ABI method exists)
- Reworking `title`/search-cache semantics — saved_manga is parallel and new

## Decisions

- **D1 — New `saved_manga` table, `title` untouched.** Columns: `id` pk, `plugin`, `slug`, `name`, `cover`, `added_at` (unix seconds). Unique index on `(plugin, slug)` — upsert via insert-or-update. Why: `title` joins to `plugin` with FK constraints; saved rows are self-describing (name/cover denormalized) so the grid never calls plugins at render time; FK to `plugin` would couple saves to plugin registration lifetime.
- **D2 — Un-save = hard delete**, not a soft flag. Why: no history requirements today; simplest correct model.
- **D3 — Grid shows saved entries only when any exist; otherwise the empty state**, not the seed grid. Why: user confirmed home == their library; seed rows are a search cache, not curation.
- **D4 — Toggle state hydrated client-side** on the detail page (`GET /api/library` membership check or a boolean exposed alongside `hint`). Why: detail renders server-side but membership is per-user store state, not page-static data; one tiny JSON fetch fits the Alpine pattern. Avoids stuffing store queries into the page-render path.
- **D5 — `POST /api/library` and `POST /api/library/remove`** (mirror the existing mutating-route convention: all writes are POSTs in this codebase; CBZ export etc. aside). DELETE avoided: Crow DELETE works but the codebase never uses it for JSON bodies — stay consistent.
- **D6 — added_at ordering, covers via `/cache/img`** with the same `?cover=1` pattern as the detail page, so the grid degrades identically when a cover 404s (existing `@error` handler pattern).

## Risks / Trade-offs

- [saved_manga rows keep stale name/cover if plugin metadata changes upstream] → acceptable; re-save refreshes via upsert; user can re-toggle
- [Home grid currently paginated/filtered by plugin + q] → saved grid reuses the same card markup without the search-cache pagination semantics; pagination for saved lists deferred until a library exceeds a screen
- [Boot migration for a new table] → created by `create_storage()` automatically on next boot; fresh table, no backfill possible from `title` (it lacks slug/cover semantics)

## Migration Plan

Additive. Rollback = revert; dropping `saved_manga` loses saved entries (documented, acceptable).
