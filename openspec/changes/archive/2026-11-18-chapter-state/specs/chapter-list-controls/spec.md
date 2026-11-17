# chapter-list-controls Delta

## MODIFIED Requirements

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

## ADDED Requirements

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
