# Spec Delta: reader-page

## MODIFIED Requirements

### Requirement: Pages API serves host-owned image URLs
`GET /api/pages/<plugin>/<slug>/<ch>` SHALL resolve the chapter entry and SHALL answer from the
stored `chapter_pages` rows for that release whenever rows exist, without calling the plugin. Only a
release with no stored page rows SHALL go through the cached-or-live wrapper, and a successful parse
SHALL write the rows back. Locally served responses SHALL have the same shape as a parse: page URLs
in `pages[]` as host-owned `/cache/img?...` entries carrying the chapter id, `chapter`, and the
adjacent chapters as both numbers (`prev_ch`, `next_ch`) and chapter ids
(`prev_ch_id`, `next_ch_id`), resolved by position in a stable number-ordered list rather than by the
plugin's emitted order. The response SHALL accept an optional `ch_id` query parameter: when `ch_id`
matches an entry in the chapter list, that entry SHALL be the target; when `ch_id` is absent or
matches nothing, the target SHALL be the first entry whose `number` equals `<ch>`. Serving locally
SHALL NOT require the chapter's images to be complete on disk.

#### Scenario: Pages list in reading order
- **WHEN** the plugin returns a page list for the resolved chapter id
- **THEN** `pages[]` is emitted in plugin order, each entry a `/cache/img` URL carrying the chapter id

#### Scenario: Opened chapter never calls the plugin
- **WHEN** the target release has `chapter_pages` rows and the request is issued again
- **THEN** the response is built from those rows and no plugin call or outbound request occurs

#### Scenario: Partially cached chapter stays local
- **WHEN** the target release has rows but only some of its page images are on disk
- **THEN** the route still answers from the rows, and only the missing page images are fetched on demand

#### Scenario: Cold chapter falls through to the plugin
- **WHEN** the target release has no stored page rows
- **THEN** the request goes through the cached-or-live plugin path and the rows are written on success

#### Scenario: Local serve schedules one revalidation
- **WHEN** a release is served from rows while the chapter is not fully on disk
- **THEN** one background re-parse of that release's page list is scheduled for the pair, and a second
  open of the same release in the same process schedules none

#### Scenario: Unknown chapter is 404
- **WHEN** no list entry matches the requested number and no `ch_id` matches
- **THEN** the route answers 404 with `{"ok":false,"error":"unknown chapter"}`

#### Scenario: Chapter id selects the exact duplicate
- **WHEN** the chapter list holds two entries numbered 24 and the request carries the second entry's `ch_id`
- **THEN** that entry's stored pages are served, and the plugin is asked for its id only when it has no rows

#### Scenario: Number-only request keeps legacy behaviour
- **WHEN** the request carries no `ch_id`
- **THEN** the target is the first entry with that number and the response shape is unchanged plus the id fields

#### Scenario: Duplicate numbers are separate stops
- **WHEN** the list holds two entries numbered 24 and the target is the first of them
- **THEN** `next_ch` is 24 and `next_ch_id` is the second entry's id, and the reverse holds for `prev`

## ADDED Requirements

### Requirement: Reader prefetch resolves metadata locally
The reader window composition (`reader_window`) SHALL build its chapter list from the stored
`chapters` rows and each chapter's page list from the stored `chapter_pages` rows. The cached-or-live
plugin path SHALL remain only for a title with no stored chapter rows. The prefetch worker's behavior
SHALL NOT change: it SHALL fetch only page images that are absent from disk.

#### Scenario: Prefetch on a stored title makes no metadata request
- **WHEN** the reader posts a position for a chapter of a title that has stored chapters and page rows
- **THEN** the returned window is composed from the store and no `chapters`/`pages` plugin call occurs

#### Scenario: Prefetch still fills missing images
- **WHEN** the window contains pages that are not on disk
- **THEN** the worker fetches exactly those pages and leaves cached pages untouched
