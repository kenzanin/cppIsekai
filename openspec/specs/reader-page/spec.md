# reader-page Specification

## Purpose

Reader UI for cppIsekai: four reading modes (webtoon, paged LTR/RTL, 2-page horizontal spread RTL-aware, 2-page vertical), a host-rendered pages API, and per-manga position persistence with resume.

## Requirements

### Requirement: Reader page four reading modes
The reader at `GET /read/<plugin>/<slug>?ch=<n>` SHALL render chapter pages in the selected reading mode: `webtoon` (vertical scroll, one page per row, lazy-loaded), `paged` with direction `ltr` or `rtl` (one page per screen), `spread-h` (two pages side by side, direction `ltr` or `rtl`), or `spread-v` (two pages stacked vertically, paged downward). The selected mode and direction SHALL persist per manga and be restored on reopen; the user's explicit mode choice SHALL override any plugin long-strip hint. Default SHALL be `webtoon` when the plugin flags long-strip content, otherwise `paged rtl`.

#### Scenario: Spread pairing odd page count
- **WHEN** chapter pages 1..5 are read in `spread-h` mode
- **THEN** page 1 renders alone, pages (2,3) pair, page (4,5) pair
- **AND** in `rtl` direction pair order mirrors (right page is the lower-numbered one)

#### Scenario: 2-page vertical mode pages downward
- **WHEN** the user advances in `spread-v` mode
- **THEN** the next two pages render stacked top-to-bottom
- **AND** the final unpaired page renders alone

#### Scenario: Mode persists per manga
- **WHEN** the user switches a manga to `spread-v` and later reopens it
- **THEN** the reader restores `spread-v` mode and last position

#### Scenario: User override beats plugin hint
- **WHEN** the plugin flags long-strip but the user chose `paged`
- **THEN** the reader opens in `paged` mode

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

### Requirement: Position persistence and resume
`POST /reader/position` SHALL persist {plugin, slug, chapter, page, mode, direction} per manga in the store (upsert; absent mode/direction fields keep stored values) and SHALL drive the prefetch worker. Reopening a manga SHALL resume the persisted chapter and page, and the detail page SHALL show a continue hint ("continue ch N p M") when a position exists.

#### Scenario: Resume after restart
- **WHEN** a manga is left at ch 12 p 3 and the daemon restarts
- **AND** the user reopens the manga
- **THEN** the reader resumes at ch 12 p 3

#### Scenario: Position update nudges prefetch
- **WHEN** the client posts a new position
- **THEN** the worker prefetches the sliding window from the position
- **AND** subsequent page fetches hit disk, not network

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

### Requirement: Chapter-switch URL synchronization
An in-place chapter switch SHALL update the browser URL via `history.pushState` (chapter and page query), and `popstate` SHALL restore the matching chapter and page in place, so refresh, back/forward, and shared links deep-link to the same chapter and page as a full load would.

#### Scenario: Refresh after in-place switch
- **WHEN** the user switched to chapter 5 page 2 in place and refreshes
- **THEN** the reader loads chapter 5 page 2 directly

#### Scenario: Back returns to previous chapter
- **WHEN** the user switched chapters in place and presses the browser back button
- **THEN** the previous chapter and page restore in place without a document reload

### Requirement: Adjacent chapter prefetch
After a chapter renders, the reader SHALL prefetch the page lists of the adjacent chapters (next first, then previous) in the background. Switching to a prefetched chapter SHALL render without waiting on a network fetch.

#### Scenario: Prefetch after render
- **WHEN** a chapter has finished rendering and both adjacent chapters exist
- **THEN** the next chapter's page list is fetched, then the previous chapter's

#### Scenario: Instant switch from prefetch
- **WHEN** the user advances to the next chapter after its page list was prefetched
- **THEN** the chapter renders without an additional fetch for its page list

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
