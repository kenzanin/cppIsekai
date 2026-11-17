# chapter-list-controls Specification

## Purpose

Per-chapter read/skip controls on the manga detail page: paginated chapter list with per-row read-progress tracking (eye = mark read, double-check = done), counters, and stored state under `host/read/` + `host/skip/` plugin_state keys.

## Requirements

### Requirement: Chapter list pagination
The detail page SHALL render the chapter list in pages of a fixed size with prev/next controls and a position indicator; the controls SHALL be hidden when the whole list fits one page.

#### Scenario: Long list paginates
- **WHEN** the chapter list holds 45 entries at 20 per page
- **THEN** the first page shows 20 rows, the indicator reads 1/3, and next/prev move one page

#### Scenario: Short list hides pager
- **WHEN** the list holds 12 entries
- **THEN** no pager controls render

### Requirement: Per-chapter skip toggle

Each chapter row SHALL render a skip control in the shape of an eye icon (open = not skipped,
closed = skipped), initialized from the `chapters.flags` bit1, toggling the bit through
`POST /chapter/flags`.

#### Scenario: Eye mirrors stored flag

- **WHEN** chapter X has bit1 set
- **THEN** its row renders the closed eye

#### Scenario: Toggle persists

- **WHEN** the eye is clicked
- **WHEN** the page is reloaded
- **THEN** the flag flipped on the first click is still reflected

### Requirement: Read-completion check

Each row SHALL render a check control lit iff the chapter's `read` bit (bit0) is set. The host
SHALL set bit0 when a progress record reaches the chapter's page count; bulk actions SHALL set
or clear it directly.

#### Scenario: Reading completes the chapter

- **WHEN** the reader records the last page of the chapter
- **THEN** the host sets bit0 and the check renders lit on the next list render

#### Scenario: Unfinished chapter

- **WHEN** the stored progress (3) is below the chapter page count (9) and no read bit is set
- **THEN** the check renders unlit

### Requirement: Read progress counter
Each row SHALL display `pages read / page count` from the same stored value that drives the check; a chapter without a record renders `0/total`.

#### Scenario: Counter matches stored state
- **WHEN** a chapter has 12 pages with 5 recorded as read
- **THEN** its row shows 5/12

### Requirement: Bulk read actions on the detail page

The chapter list header SHALL offer "mark all read" and "clear read status"; each row SHALL
offer "mark previous read" which, after a confirm dialog naming the affected range, resolves
the id set client-side from the loaded chapter list and issues one `POST /chapter/flags`.

#### Scenario: Mark all read

- **WHEN** mark-all is confirmed on a 45-chapter manga
- **THEN** one flags request marks all 45 and every check renders lit without reload

#### Scenario: Mark previous read

- **WHEN** "mark previous" is invoked on chapter 10 of 30
- **THEN** a confirm names chapters 1–10, and on confirm one flags request sets bit0 for
  exactly those ten ids

#### Scenario: Clear all keeps progress labels

- **WHEN** clear-all is confirmed
- **THEN** every check renders unlit while per-row progress counts remain displayed
