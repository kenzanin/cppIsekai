# manga-detail-page Specification

## Purpose

Give the daemon a per-manga detail page: one place that renders a title's metadata and chapter list from its plugin, serves it stale-first, lets the user force a refresh, and hands each chapter off to reading and cbz download.

## Requirements

### Requirement: Detail page rendering
The daemon SHALL expose `GET /view/manga/<plugin>/<slug>` rendering the detail page shell from `templates/detail.mustache`; the page SHALL hydrate title metadata and the chapter list via the detail/chapters JSON APIs, degrade per-field when fields are absent, and search result cards SHALL link into it.

#### Scenario: Detail page renders from plugin data
- **WHEN** a user opens `/view/manga/mangadex/<slug>` for a known title
- **THEN** the page renders cover, title, status, tags, and the chapter list
- **AND** each chapter row offers read and cbz links

#### Scenario: Missing fields degrade per-field
- **WHEN** the plugin omits optional metadata fields
- **THEN** those fields render as placeholders instead of breaking the page

### Requirement: Stale-serve on detail and chapters data
`GET /api/detail/<plugin>/<slug>` and `GET /api/chapters/<plugin>/<slug>` SHALL serve cached data with `cached:true` when the plugin call fails, SHALL 404 with `{"ok":false,"error":"unknown title"}` on invalid slug/plugin pairs, and SHALL never crash the host on plugin errors.

#### Scenario: Cached serve when upstream fails
- **WHEN** the plugin call fails but a previous response is cached
- **THEN** the endpoint returns the cached envelope plus `cached:true`

#### Scenario: Unknown title guard
- **WHEN** the slug or plugin does not resolve in the store
- **THEN** the endpoint returns 404 with the unknown-title error body

### Requirement: Forced refresh endpoint
`POST /api/refresh/<plugin>/<slug>` SHALL call the plugin's detail and chapters methods, SHALL overwrite both cache entries only after BOTH envelopes validate, SHALL return `{"ok":true}` on success, and SHALL return HTTP 502 with a refresh-failed error without touching the cache on plugin failure.

#### Scenario: Successful refresh overwrites cache
- **WHEN** both plugin envelopes validate
- **THEN** both cache entries are replaced and the endpoint returns ok

#### Scenario: Failed refresh leaves cache intact
- **WHEN** either plugin call fails or an envelope fails validation
- **THEN** the endpoint returns 502 naming the failed method
- **AND** the existing cache entries are unchanged

### Requirement: Per-chapter cbz hand-off
The detail page chapter rows SHALL link to the cbz export endpoint so each chapter can be downloaded as a cbz without leaving the page.

#### Scenario: Chapter cbz link
- **WHEN** a chapter row's cbz link is followed
- **THEN** the cbz export flow for that chapter is invoked with plugin and chapter identifiers
