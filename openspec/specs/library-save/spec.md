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
`GET /api/library` SHALL return the saved entries (name, cover, plugin, slug) ordered by `added_at` descending. The home/Library grid SHALL show saved entries as cover cards linking to `/view/manga/<plugin>/<slug>`; an empty library SHALL show the empty state instead of the search-cache grid.

#### Scenario: Saved entry appears on home
- **WHEN** a manga is saved and the home page is opened
- **THEN** its cover card appears and links to its detail page

#### Scenario: Unsaved entry disappears
- **WHEN** a saved entry is unsaved and the home page reloads
- **THEN** its card is gone

#### Scenario: Library vs search cache
- **WHEN** the `title` search-cache table contains seed rows but nothing is saved
- **THEN** the Library grid renders the empty state, not the search cache

#### Scenario: Saved view ignores search filters (2026-11-10)
- **WHEN** the request carries `?plugin=`, `?q=`, or `?page=` and saved entries exist
- **THEN** the grid renders the saved entries in `added_at` desc order without pagination controls, regardless of the filter values

### Requirement: Toggle state is hydrated client-side
The detail page SHALL initialize its toggle from `GET /api/library` membership (plugin+slug) at boot, mirroring the existing `loadPluginMeta()` pattern; it SHALL NOT rely on a server-side `saved` field on the detail response.

#### Scenario: Saved manga reloaded
- **WHEN** the detail page for a saved manga is loaded
- **THEN** the button renders "In library" on first paint (after the membership fetch resolves before any user interaction)

#### Scenario: Toggle failure
- **WHEN** `POST /api/library` or `POST /api/library/remove` fails
- **THEN** the error surfaces on the detail page's existing `err` line and the button label stays consistent with the last known state
