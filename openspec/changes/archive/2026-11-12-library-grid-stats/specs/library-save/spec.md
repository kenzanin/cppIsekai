## MODIFIED Requirements

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
