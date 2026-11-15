# manga-detail-page Specification

## Purpose

Give the daemon a per-manga detail page: one place that renders a title's metadata and chapter list from its plugin, serves it stale-first, lets the user force a refresh, and hands each chapter off to reading and cbz download.

## Requirements

### Requirement: Detail page rendering
The daemon SHALL expose `GET /view/manga/<plugin>/<slug>` rendering the detail page shell from `templates/detail.mustache`; the page SHALL hydrate title metadata and the chapter list via the detail/chapters JSON APIs, degrade per-field when fields are absent, and search result cards SHALL link into it. `GET /api/detail/<plugin>/<slug>` SHALL serve a saved manga's stored detail from its `saved_manga` row without calling the plugin, marked `cached`, and SHALL keep carrying the `enrichment` block and `chosen_cover`. Titles that are not saved SHALL keep serving the plugin envelope through the stale layer. Chapter data SHALL keep its current live/stale-serve behaviour in both cases.

#### Scenario: Detail page renders from plugin data
- **WHEN** a user opens `/view/manga/mangadex/<slug>` for a known title
- **THEN** the page renders cover, title, status, tags, and the chapter list
- **AND** each chapter row offers read and cbz links

#### Scenario: Missing fields degrade per-field
- **WHEN** the plugin omits optional metadata fields
- **THEN** those fields render as placeholders instead of breaking the page

#### Scenario: Saved manga reads its row
- **WHEN** `GET /api/detail` targets a saved manga with stored detail and the plugin is unreachable
- **THEN** the response carries the stored metadata, is flagged `cached`, and requires no plugin call

#### Scenario: Picker overlay survives read-first
- **WHEN** `GET /api/detail` answers a saved manga from its row
- **THEN** the response still carries `enrichment.fetched_at`, `enrichment.chosen` and `chosen_cover`

#### Scenario: Disabled plugin still serves the row
- **WHEN** the plugin of a saved manga is disabled and the detail page is opened
- **THEN** the stored detail is served and `POST /api/refresh` answers `plugin disabled`

#### Scenario: Cold miss materializes
- **WHEN** a saved manga has no stored detail yet and the detail page is opened
- **THEN** the envelope is resolved as before and the result is written to the row

#### Scenario: Unsaved manga stays live
- **WHEN** `GET /api/detail` targets a title that is not in the library
- **THEN** the plugin envelope (or its stale copy) is served as before

#### Scenario: Refresh rewrites the row
- **WHEN** the user triggers `POST /api/refresh/<plugin>/<slug>` for a saved manga
- **THEN** the plugin is called, both cache entries are overwritten after validation, and the stored display detail is rewritten from the new envelope

### Requirement: Stale-serve on detail and chapters data
`GET /api/detail/<plugin>/<slug>` and `GET /api/chapters/<plugin>/<slug>` SHALL serve cached data with `cached:true` when the plugin call fails, SHALL 404 with `{"ok":false,"error":"unknown title"}` on invalid slug/plugin pairs, and SHALL never crash the host on plugin errors.

#### Scenario: Cached serve when upstream fails
- **WHEN** the plugin call fails but a previous response is cached
- **THEN** the endpoint returns the cached envelope plus `cached:true`

#### Scenario: Unknown title guard
- **WHEN** the slug or plugin does not resolve in the store
- **THEN** the endpoint returns 404 with the unknown-title error body

### Requirement: Forced refresh endpoint
`POST /api/refresh/<plugin>/<slug>` SHALL call the plugin's detail and chapters methods, SHALL overwrite both cache entries only after BOTH envelopes validate, SHALL return `{"ok":true}` on success, and SHALL return HTTP 502 with a refresh-failed error without touching the cache on plugin failure. Regardless of outcome, it SHALL stamp the `update_attempts` tracker for `(plugin, slug)` with the observed result (`ok` | `error`) so the auto-update scheduler treats a manual refresh as a fresh attempt and skips the title for the configured interval.

#### Scenario: Successful refresh overwrites cache
- **WHEN** both plugin envelopes validate
- **THEN** both cache entries are replaced and the endpoint returns ok
- **AND** `update_attempts(plugin, slug)` records `outcome = ok` with the current timestamp

#### Scenario: Failed refresh leaves cache intact
- **WHEN** either plugin call fails or an envelope fails validation
- **THEN** the endpoint returns 502 naming the failed method
- **AND** the existing cache entries are unchanged
- **AND** `update_attempts(plugin, slug)` records `outcome = error` with the failure reason

#### Scenario: Manual refresh exempts title from auto-update
- **WHEN** the user clicks the detail-page refresh button and the call completes (either outcome)
- **THEN** the auto-update scheduler does not select that title again until its next interval elapses

### Requirement: Per-chapter cbz hand-off
The detail page chapter rows SHALL link to the cbz export endpoint so each chapter can be downloaded as a cbz without leaving the page.

#### Scenario: Chapter cbz link
- **WHEN** a chapter row's cbz link is followed
- **THEN** the cbz export flow for that chapter is invoked with plugin and chapter identifiers

### Requirement: Icon-only controls expose accessible names
Every icon-only or image-only interactive control rendered by the detail page and the
library grid SHALL carry an accessible name, including the per-card plugin control, the
status filter controls, the dim toggle and the reader pager buttons. A control whose only
content is an icon SHALL NOT rely on a `title` attribute alone.

#### Scenario: Icon-only control has a name
- **WHEN** the detail page or the library grid renders a control whose content is only an icon
- **THEN** that control exposes an accessible name

#### Scenario: Named control still renders
- **WHEN** an icon-only control gains an accessible name
- **THEN** its visible glyph and layout are unchanged

### Requirement: Focus indication stays visible
A control that receives keyboard focus on the templates' dark surfaces SHALL render a
visible focus indicator. Removing the outline without supplying a visible replacement SHALL
NOT be treated as meeting this requirement.

#### Scenario: Focus stays visible
- **WHEN** a control receives keyboard focus on a dark surface
- **THEN** a visible focus indicator is rendered
