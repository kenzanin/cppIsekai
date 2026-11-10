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
Each chapter row SHALL render a skip control in the shape of an eye icon (open = not skipped, closed = skipped), initialized from stored state, toggling the stored flag per chapter.

#### Scenario: Eye mirrors stored flag
- **WHEN** chapter X has skip flag "1"
- **THEN** its row renders the closed eye

#### Scenario: Toggle persists
- **WHEN** the eye is clicked
- **WHEN** the page is reloaded
- **THEN** the flag flipped on the first click is still reflected

### Requirement: Read-completion check
Each row SHALL render a double-check control that is lit iff stored pages-read >= the chapter's page count, toggled by click, and bumped automatically by reader position records for that chapter.

#### Scenario: Unfinished chapter
- **WHEN** stored read pages (3) are below the chapter total (9)
- **THEN** the check renders unlit; clicking it marks the chapter read and lights it

#### Scenario: Reading completes the chapter
- **WHEN** the reader records page 9 of 9 for the chapter
- **THEN** the chapter's check is lit on the next detail visit without manual action

### Requirement: Read progress counter
Each row SHALL display `pages read / page count` from the same stored value that drives the check; a chapter without a record renders `0/total`.

#### Scenario: Counter matches stored state
- **WHEN** a chapter has 12 pages with 5 recorded as read
- **THEN** its row shows 5/12
