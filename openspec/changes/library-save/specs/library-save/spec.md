# Spec Delta: library-save

## ADDED Requirements

### Requirement: Save and unsave a manga
`POST /api/library` SHALL upsert a `saved_manga` row `{plugin, slug, name, cover, added_at}` (unique on `(plugin, slug)`); `DELETE /api/library` (or equivalent remove route) SHALL delete it. The response SHALL report the resulting state (`saved: true|false`).

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
`GET /api/library` SHALL return the saved entries (name, cover, plugin, slug) ordered by `added_at` descending. The home/Library grid SHALL show saved entries as cover cards linking to `/view/manga/<plugin>/<slug>`; an empty library SHALL show an empty-state message instead of the search-cache grid.

#### Scenario: Saved entry appears on home
- **WHEN** a manga is saved and the home page is opened
- **THEN** its cover card appears and links to its detail page

#### Scenario: Unsaved entry disappears
- **WHEN** a saved entry is unsaved and the home page reloads
- **THEN** its card is gone

#### Scenario: Library vs search cache
- **WHEN** the `title` search-cache table contains seed rows but nothing is saved
- **THEN** the Library grid renders the empty state, not the search cache
