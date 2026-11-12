# Spec Delta

## ADDED Requirements

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
