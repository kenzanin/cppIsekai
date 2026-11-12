# library-save Specification

## Purpose

Save manga to a personal library: a `saved_manga` store table, JSON routes to save/unsave/list, a Library grid on the home page rendering saved entries as cover cards, and a client-side toggle on the detail page hydrated from `GET /api/library` membership.

## Requirements

### Requirement: Save and unsave a manga
`POST /api/library` SHALL upsert a `saved_manga` row `{plugin, slug, name, cover, added_at}` (unique on `(plugin, slug)`); `POST /api/library/remove` SHALL delete it. The response SHALL report the resulting state (`saved: true|false`).

#### Scenario: Add to library toggles on
- **WHEN** the user clicks "Add to library" on a detail page that is not saved
- **THEN** the entry is persisted and the button becomes "In library"

#### Scenario: Re-add is idempotent
- **WHEN** `POST /api/library` targets an already-saved entry
- **THEN** the row is updated (name/cover refreshed) without duplicating rows

#### Scenario: Remove from library toggles off
- **WHEN** the user clicks "In library" on a saved entry
- **THEN** the entry is deleted and the button becomes "Add to library"

### Requirement: Saved entries are listed
`GET /api/library` SHALL return the saved entries (name, cover, plugin, slug, and the stat fields `status, total_ch, read_ch, has_update, last_read_at, updated_at`) ordered by `added_at` descending. The home/Library grid SHALL render saved entries as cover cards linking to `/view/manga/<plugin>/<slug>`; an empty library SHALL show the empty state instead of the search-cache grid. In saved mode the grid is client-paginated (12/page) instead of server-paginated.

#### Scenario: Saved entry appears on home
- **WHEN** a manga is saved and the home page is opened
- **THEN** its cover card appears with its stat fields in the SSR context

#### Scenario: Unsaved entry disappears
- **WHEN** a saved entry is unsaved and the home page reloads
- **THEN** its card is gone

#### Scenario: Library vs search cache
- **WHEN** the `title` search-cache table contains seed rows but nothing is saved
- **THEN** the Library grid renders the empty state, not the search cache

#### Scenario: Saved view ignores search filters (2026-11-10)
- **WHEN** the request carries `?plugin=`, `?q=`, or `?page=` and saved entries exist
- **THEN** the grid renders the saved entries regardless of the filter values (client-side controls may then apply `?plugin=` as the initial chip)

#### Scenario: Deep-link plugin filter
- **WHEN** the home page is opened with `?plugin=<id>` and saved entries exist
- **THEN** the grid pre-filters to that plugin client-side

### Requirement: Saved entries store duplicate signal
Saved entries SHALL store the source plugin's `synopsis` and `alt_titles` (JSON array, verbatim) when present in the detail envelope; they are refreshed on save and on detail refresh; enrichment values are NOT used for this.

#### Scenario: Synopsis stored on refresh
- **WHEN** refresh returns `detail.synopsis`
- **THEN** the saved row stores it

### Requirement: FTS5 duplicate grouping
The store SHALL maintain an FTS5 index over saved-entry `name`, `alt_titles`, `synopsis`; on save/unsave/refresh it SHALL recompute `dup_group` (0 = unique) via FTS5 candidate generation + token-Dice similarity ≥ 0.6 in connected components. If FTS5 is unavailable, `dup_group` stays 0 and the duplicate filter reports no duplicates (no crash).

#### Scenario: Cross-plugin duplicates grouped
- **WHEN** two saved entries from different plugins share normalized alt-title tokens and similar synopsis
- **THEN** both rows get the same non-zero `dup_group`

#### Scenario: Identical title same site grouped
- **WHEN** the same entry is saved unsubscribe/re-saved (same plugin, same slug) it is one row — n/a; duplicate means different slug/plugin with same work

### Requirement: Duplicate filter clusters adjacently
In the Library grid, activating the "duplicate" chip SHALL show only rows with non-zero `dup_group` and SHALL render each cluster contiguously (client-side sort by `dup_group`, then name).

#### Scenario: Adjacent duplicates
- **WHEN** three cluster members exist among other unique entries
- **THEN** the three render consecutively under the duplicate filter

### Requirement: Grid exposes dup marker
Each saved card SHALL carry `data-dup=<dup_group>` so the grid can filter/sort without extra requests.

#### Scenario: Unique entries hidden under filter
- **WHEN** the chip is active and only 2 of 50 saved rows have non-zero `dup_group`
- **THEN** exactly those 2 render

### Requirement: Toggle state is hydrated client-side
The detail page SHALL initialize its toggle from `GET /api/library` membership (plugin+slug) at boot, mirroring the existing `loadPluginMeta()` pattern; it SHALL NOT rely on a server-side `saved` field on the detail response.

#### Scenario: Saved manga reloaded
- **WHEN** the detail page for a saved manga is loaded
- **THEN** the button renders "In library" on first paint (after the membership fetch resolves before any user interaction)

#### Scenario: Toggle failure
- **WHEN** `POST /api/library` or `POST /api/library/remove` fails
- **THEN** the error surfaces on the detail page's existing `err` line and the button label stays consistent with the last known state
