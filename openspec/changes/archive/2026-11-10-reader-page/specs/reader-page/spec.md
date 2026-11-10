# Spec Delta: reader-page

## ADDED Requirements

### Requirement: Reader page with four reading modes

The reader at `GET /view/manga/<plugin>/<slug>?ch=<n>` SHALL render the chapter's pages in the selected reading mode: `webtoon` (vertical scroll, one page per row, lazy-loaded), `paged` with direction `ltr` or `rtl` (one page per screen), `spread-h` (two pages side by side, direction `ltr` or `rtl`), or `spread-v` (two pages stacked vertically, paged downward). The selected mode and direction SHALL persist per manga and be restored on reopen; the user's explicit mode choice SHALL override any plugin long-strip hint. Default SHALL be `webtoon` when the plugin flags long-strip content, otherwise `paged rtl`.

#### Scenario: Spread pairing with odd page count

- **WHEN** a chapter with pages 1..5 is read in `spread-h` mode
- **THEN** page 1 renders alone, pages (2,3) pair, page (4,5) pair
- **AND** in `rtl` direction the pair order mirrors (right page is the lower-numbered one)

#### Scenario: 2-page vertical mode pages downward

- **WHEN** the user advances in `spread-v` mode
- **WHEN** advancing from a stacked pair
- **THEN** the next two pages render stacked top-to-bottom
- **AND** a final unpaired page renders alone

#### Scenario: Mode persists per manga

- **WHEN** the user switches a manga to `spread-v` and later reopens it
- **THEN** the reader restores `spread-v` mode and the last position

#### Scenario: User override beats plugin hint

- **WHEN** the plugin flags long-strip but the user chose `paged`
- **THEN** the reader opens in `paged` mode

### Requirement: Pages API serves host-owned image URLs

`GET /api/pages/<plugin>/<slug>/<ch>` SHALL return an ordered list of `/cache/img` URLs for the chapter (host-rendered: the client SHALL NOT construct cache paths itself), plus `next_ch` and `prev_ch` chapter numbers for navigation. An unknown chapter SHALL be answered with a 404 JSON error. Page metadata SHALL come from the plugin `pages` call through the stale layer.

#### Scenario: Pages list for a cached chapter

- **WHEN** `GET /api/pages/mangadex/<slug>/3` is called for a known chapter
- **THEN** the response lists one `/cache/img` URL per page in reading order
- **AND** carries `next_ch` and `prev_ch`

#### Scenario: Unknown chapter

- **WHEN** `GET /api/pages/mangadex/<slug>/999` targets a chapter absent from the plugin's chapter list
- **THEN** the response is 404 JSON

### Requirement: Position persistence and resume

`POST /reader/position` SHALL persist {plugin, slug, chapter, page, mode, direction} per manga in the store (upsert; absent mode/direction fields keep stored values) and SHALL drive the prefetch worker as today. Reopening a manga SHALL resume at the persisted chapter and page, and the detail page SHALL show a continue hint ("continue ch N p M") when a position exists.

#### Scenario: Resume after restart

- **WHEN** a manga is left at ch 12 p 3 and the daemon restarts
- **WHEN** the user reopens the manga
- **THEN** the reader resumes at ch 12 p 3

#### Scenario: Position update nudges prefetch

- **WHEN** the client posts a new position
- **THEN** the worker prefetches the sliding window from that position
- **AND** subsequent page fetches hit disk, not the network
