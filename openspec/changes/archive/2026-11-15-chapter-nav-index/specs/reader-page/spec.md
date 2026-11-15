# Spec Delta: reader-page

## MODIFIED Requirements

### Requirement: Pages API serves host-owned image URLs
`GET /api/pages/<plugin>/<slug>/<ch>` SHALL resolve the chapter entry, fetch its
page list through the stale-serve wrapper, and return host-owned image URLs
(`/cache/img?...`) built from that entry's chapter id. It SHALL accept an
optional `ch_id` query parameter: when `ch_id` matches an entry in the chapter
list, that entry SHALL be the target (its id is what the plugin is asked for,
what the image URLs carry, and what the response identifies); when `ch_id` is
absent or matches nothing, the target SHALL be the first entry whose `number`
equals `<ch>`, as before. The response SHALL carry the chapter number in
`chapter`, the page URLs in `pages[]`, and the adjacent chapters as both numbers
(`prev_ch`, `next_ch`) and chapter ids (`prev_ch_id`, `next_ch_id`). Adjacent
chapters SHALL be resolved by position in a stable number-ordered list, not by
the plugin's emitted order, so duplicated chapter numbers yield one navigation
stop per entry.

#### Scenario: Pages list in reading order
- **WHEN** the plugin returns a page list for the resolved chapter id
- **THEN** `pages[]` is emitted in plugin order, each entry a `/cache/img` URL carrying the chapter id

#### Scenario: Unknown chapter is 404
- **WHEN** no list entry matches the requested number and no `ch_id` matches
- **THEN** the route answers 404 with `{"ok":false,"error":"unknown chapter"}`

#### Scenario: Chapter id selects the exact duplicate
- **WHEN** the chapter list holds two entries numbered 24 and the request carries the second entry's `ch_id`
- **THEN** the plugin is asked for that id and the served pages are that entry's pages

#### Scenario: Number-only request keeps legacy behaviour
- **WHEN** the request carries no `ch_id`
- **THEN** the target is the first entry with that number and the response shape is unchanged plus the new id fields

#### Scenario: Duplicate numbers are separate stops
- **WHEN** the list holds two entries numbered 24 and the target is the first of them
- **THEN** `next_ch` is 24 and `next_ch_id` is the second entry's id, and the reverse holds for `prev`

### Requirement: In-place chapter switch
The reader SHALL switch chapters by chapter id: `/api/pages` requests carry the
target `ch_id`, the address bar SHALL keep both `ch` (number, for display and
share links) and `ch_id` (exact release), and the prefetch cache SHALL be keyed
by chapter id so two entries sharing a number do not overwrite each other. A
switch SHALL update the displayed number, page list, and neighbours from the
response without a page reload, and the position store SHALL keep keying by
chapter id.

#### Scenario: Next chapter without reload
- **WHEN** the reader advances past the last page
- **THEN** it requests `/api/pages` with `next_ch_id` and renders that chapter in place

#### Scenario: Previous chapter without reload
- **WHEN** the reader goes back from the first page
- **THEN** it requests `/api/pages` with `prev_ch_id` and renders that chapter in place

#### Scenario: Chapter dropdown switch
- **WHEN** the user picks a chapter from the dropdown
- **THEN** the request carries that entry's id and the number shown matches the response

#### Scenario: Sibling release is a neighbour
- **WHEN** the current chapter is one of two entries sharing a number
- **THEN** the neighbour in that direction is the sibling entry, reachable without going through the detail page

#### Scenario: Mode and zoom survive switch
- **WHEN** the chapter changes
- **THEN** reading mode and zoom keep their values

## ADDED Requirements

### Requirement: Duplicate chapter numbers stay individually reachable
Every entry in a plugin's chapter list SHALL be reachable from the reader's
own navigation, regardless of duplicated numbers, without editing the URL or
returning to the detail page. Chapter numbers SHALL be treated as labels, never
as identity: identity is the plugin's chapter id.

#### Scenario: Read every release of a duplicated number
- **WHEN** a title lists two entries numbered 24 and the reader steps forward from the first
- **THEN** the second entry's pages are served, and stepping forward again leaves number 24

#### Scenario: Refresh keeps the release
- **WHEN** the reader reloads the page while reading the second entry numbered 24
- **THEN** the served pages are that same entry's pages, not the first entry's
