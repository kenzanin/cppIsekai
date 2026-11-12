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
`GET /api/pages/<plugin>/<slug>/<ch>` SHALL return an ordered list of `/cache/img` URLs for the chapter (host-rendered: the client SHALL NOT construct cache paths itself), plus next/previous chapter references. Unknown chapters SHALL return 404 JSON.

#### Scenario: Pages list in reading order
- **WHEN** the client requests a known chapter
- **THEN** the response lists the chapter's `/cache/img` URLs in reading order

#### Scenario: Unknown chapter is 404
- **WHEN** `GET /api/pages/mangadex/<slug>/999` targets a chapter absent from the plugin's chapter list
- **THEN** the response is 404 JSON

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
Chapter navigation in the reader (next/previous at page boundaries, chapter dropdown, keyboard-driven navigation) SHALL switch chapters client-side by fetching the chapter's page list and swapping the rendered content in place, without a document reload. The rendered reading state (selected mode, direction, zoom) SHALL persist across the switch, and position persistence SHALL resume from the new chapter.

#### Scenario: Next chapter without reload
- **WHEN** the user advances past the last page of the current chapter and a next chapter exists
- **THEN** the next chapter's pages render in place starting at its first page
- **AND** no document reload occurs

#### Scenario: Previous chapter without reload
- **WHEN** the user steps back before the first page of the current chapter and a previous chapter exists
- **THEN** the previous chapter's pages render in place starting at its last page

#### Scenario: Chapter dropdown switch
- **WHEN** the user picks a chapter from the chapter dropdown
- **THEN** that chapter renders in place at its first page
- **AND** the dropdown selection reflects the current chapter

#### Scenario: Mode and zoom survive switch
- **WHEN** the user reads in `spread-h` rtl at a non-default zoom and switches chapter in place
- **THEN** the new chapter renders in `spread-h` rtl at the same zoom

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
