# Spec Delta: manga-detail-page

## MODIFIED Requirements

### Requirement: Detail page rendering
The daemon SHALL expose `GET /view/manga/<plugin>/<slug>` rendering the detail page shell from `templates/detail.mustache`; the page SHALL hydrate title metadata and the chapter list via the detail/chapters JSON APIs, degrade per-field when fields are absent, and search result cards SHALL link into it. `GET /api/detail/<plugin>/<slug>` SHALL serve a saved manga's metadata from its `saved_manga` row when that row has stored detail, without calling the plugin; titles that are not saved SHALL keep serving the plugin envelope through the stale layer. Chapter data SHALL keep its current live/stale-serve behaviour in both cases.

#### Scenario: Detail page renders from plugin data
- **WHEN** a user opens `/view/manga/mangadex/<slug>` for a known title
- **THEN** the page renders cover, title, status, tags, and the chapter list
- **AND** each chapter row offers read and cbz links

#### Scenario: Missing fields degrade per-field
- **WHEN** the plugin omits optional metadata fields
- **THEN** those fields render as placeholders instead of breaking the page

#### Scenario: Saved manga reads its row
- **WHEN** `GET /api/detail` targets a saved manga whose row holds stored detail and the plugin is unreachable
- **THEN** the response carries the stored metadata and does not require a plugin call

#### Scenario: Unsaved manga stays live
- **WHEN** `GET /api/detail` targets a title that is not in the library
- **THEN** the plugin envelope (or its stale copy) is served as before

#### Scenario: Refresh rewrites the row
- **WHEN** the user triggers `POST /api/refresh/<plugin>/<slug>` for a saved manga
- **THEN** the plugin is called, both cache entries are overwritten after validation, and the stored display detail is rewritten from the new envelope
