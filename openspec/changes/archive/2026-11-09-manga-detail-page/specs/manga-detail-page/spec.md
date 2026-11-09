# Spec Delta

## Purpose

Give the daemon a per-manga detail page: one place that renders a title's metadata and chapter list from its plugin, serves it stale-first, lets the user force a refresh, and hands each chapter off to reading and cbz download.

## ADDED Requirements

### Requirement: Detail page rendering
The daemon SHALL serve `GET /view/manga/<plugin>/<slug>` as an HTML page showing the manga's cover (via the local image cache proxy), title, tags, status, and description from the plugin `detail` data, and a chapter list (number, release date, cbz action, read link) from the plugin `chapters` data.

#### Scenario: Detail page renders for a known title
- **WHEN** a client requests `/view/manga/mangadex/<slug>` for a manga the plugin knows
- **THEN** the page renders the title's metadata and its full chapter list

#### Scenario: Unknown title
- **WHEN** the plugin returns a not-found result for the slug
- **THEN** the page renders a not-found response instead of a partial detail view

### Requirement: Stale-serve on detail and chapters data
The daemon SHALL serve the detail page's `detail` and `chapters` plugin data through the stale-first cache: a cached response is served immediately and a plugin failure while fresh data exists SHALL still render the page with the cached data marked as cached.

#### Scenario: Plugin down, cache warm
- **WHEN** the plugin call fails but a cached response exists for the slug
- **THEN** the page renders from cache with the cached marker visible to the client

#### Scenario: Plugin down, cache cold
- **WHEN** the plugin call fails and no cached response exists
- **THEN** the page returns an error response, not an empty shell

### Requirement: Forced refresh endpoint
The daemon SHALL expose `POST /api/refresh/<plugin>/<slug>` which re-pulls `detail` and `chapters` from the plugin bypassing the stale layer and overwrites the cached entries on success.

#### Scenario: Refresh succeeds
- **WHEN** the refresh endpoint is called and both plugin calls succeed
- **THEN** the next detail-page render reflects the newly pulled data

#### Scenario: Refresh fails
- **WHEN** the refresh endpoint is called and a plugin call fails
- **THEN** the endpoint returns a failure response and the previously cached data stays intact

### Requirement: Per-chapter cbz hand-off
Each chapter row on the detail page SHALL offer a cbz download that reuses the existing export route for that plugin and slug.

#### Scenario: Chapter cbz download
- **WHEN** the user clicks a chapter's cbz button
- **THEN** the browser downloads the cbz archive for that chapter via the existing export route
