# Spec Delta: reader-fullscreen

## ADDED Requirements

### Requirement: Full-screen reader surface
The reader route SHALL render the chapter image(s) as the only persistent on-screen content (no app header, no permanent toolbar). Navigation chrome SHALL be hidden until toggled; position/resume/zoom/mode behavior is unchanged from the base reader capability.

#### Scenario: Reader opens full screen
- **WHEN** the user opens `/read/<plugin>/<slug>?ch=<n>`
- **THEN** the image fills the viewport without the app's Library/Search header or a permanent toolbar
- **AND** a small dim `p N / M` indicator is visible at the bottom

### Requirement: Center-third tap toggles overlay nav
The middle third of the image (1/3 width, vertically centered) SHALL toggle the top and bottom overlay nav. When shown, the nav SHALL remain visible until the center is tapped again (no auto-hide). Left/right thirds SHALL continue to page previous/next (RTL-aware). Applies to all modes, including webtoon.

#### Scenario: Center tap shows nav
- **WHEN** nav is hidden and the user taps the center of the image
- **THEN** the top bar and bottom bar appear

#### Scenario: Center tap hides nav
- **WHEN** nav is shown and the user taps the center of the image
- **THEN** the top bar and bottom bar hide; left/right tap zones remain active

#### Scenario: Webtoon center tap is consistent
- **WHEN** the reader is in webtoon mode and the user taps the center
- **THEN** nav toggles exactly as in paged/spread mode

### Requirement: Overlay top bar
The top bar SHALL contain: a back control to `/view/manga/<plugin>/<slug>` (icon or link), a chapter `<select>` listing every chapter (selecting one navigates to that chapter), and a page `<select>` listing every page (selecting one jumps to that page). The back control and selects SHALL be usable only while nav is shown.

#### Scenario: Back returns to detail
- **WHEN** nav is shown and the user activates the back control
- **THEN** the browser navigates to the detail page for the current manga

#### Scenario: Chapter select navigates
- **WHEN** nav is shown and the user picks another chapter in the chapter select
- **THEN** the reader loads that chapter at its first page

#### Scenario: Page select jumps
- **WHEN** nav is shown and the user picks page N in the page select
- **THEN** the reader jumps to page N and the `/reader/position` POST reflects it

### Requirement: Overlay bottom bar with page jump
The bottom bar SHALL contain the mode, direction, and zoom selectors, plus a page-jump row of numbered buttons 1..N. The current page SHALL be highlighted. Numbered buttons SHALL render only while they fit the row (compact, on the order of two dozen); beyond that, the bottom bar SHALL fall back to a single big current-page/jump control and the full page list SHALL live in the top bar's `page <select>`.

#### Scenario: Small chapter renders numbered buttons
- **WHEN** the chapter has 12 pages and nav is shown
- **THEN** bottom bar shows buttons 1..12; button 7 is highlighted as current

#### Scenario: Large chapter falls back to jump control
- **WHEN** the chapter has 80 pages and nav is shown
- **THEN** bottom bar shows one big current-page control instead of 80 buttons; the page select in the top bar lists all 80 pages

#### Scenario: Numbered button jumps
- **WHEN** nav is shown and the user taps numbered button 5
- **THEN** the reader jumps to page 5 and the button becomes highlighted

### Requirement: Hidden-nav indicator
While nav is hidden, the reader SHALL show a small, dim `p N / M` indicator at the bottom (current page / total). It SHALL not intercept taps.

#### Scenario: Indicator shows without nav
- **WHEN** nav is hidden on page 7 of 37
- **THEN** a small dim indicator reading `p 7 / 37` (or `p 7 / 37p`) is visible at the bottom center

### Requirement: Cross-chapter tap navigation
When the user is on the last page of a chapter and taps "next", the reader SHALL navigate to the next chapter (first page). When on the first page and taps "prev", the reader SHALL navigate to the previous chapter (last page), respecting RTL direction. Only the chapter-boundary tap crosses chapters; in-chapter taps keep their step semantics.

#### Scenario: Next tap on last page advances chapter
- **WHEN** the user is on the last view of chapter 3 and taps the next zone
- **AND** the chapter list contains chapter 4
- **THEN** the reader navigates to chapter 4 page 1 with the current mode/direction/zoom

#### Scenario: Prev tap on first page goes to previous chapter
- **WHEN** the user is on the first view of chapter 3 and taps the prev zone
- **AND** the chapter list contains chapter 2
- **THEN** the reader navigates to chapter 2 at its last page

### Requirement: Chapters envelope carries page count
`GET /api/chapters/<plugin>/<slug>` SHALL include `pages` (integer, best-effort; 0 when the plugin cannot know) per chapter entry. The reader uses it to land prev-boundary navigation on the previous chapter's last page. Plain consumers ignoring the field keep working.

#### Scenario: Pages count present
- **WHEN** the plugin can report page counts per chapter
- **THEN** each `/api/chapters` entry carries `pages` >= 1

#### Scenario: Pages count absent does not break resume
- **WHEN** the plugin cannot report page counts
- **THEN** entries carry `pages: 0` and the reader falls back to landing on page 1 for prev-boundary navigation

#### Scenario: Boundary with no neighbor stops
- **WHEN** the user is on the first/last view of the first/last chapter and taps prev/next
- **THEN** nothing happens (or the Done overlay for the final chapter)

### Requirement: Done overlay at end of last chapter
When the current chapter is the last one (`next_ch` absent) and the user is on its last page and taps "next", the reader SHALL show a full-screen "Done 🎉" overlay with a button returning to `/view/manga/<plugin>/<slug>`. The overlay SHALL not navigate away by itself.

#### Scenario: Done overlay appears
- **WHEN** the user is on the last page of the last chapter and taps next
- **THEN** a full-screen "Done 🎉" overlay appears with a back-to-detail control

#### Scenario: Done overlay back control returns
- **WHEN** the Done overlay is shown and the user activates back
- **THEN** the browser navigates to the detail page

### Requirement: Mode, direction and zoom persist per manga
The reader chrome state (`mode`, `direction`, `zoom`) SHALL persist per manga in the `reader_positions` store (additive `zoom` column). `POST /reader/position` SHALL accept an optional `zoom` field (absent = keep stored, mirroring mode/direction). On every reader open the stored (or overridden via query) values SHALL apply, and chapter-to-chapter navigation SHALL carry them.

#### Scenario: Zoom persists across restart
- **WHEN** the user sets zoom to `1:1`, leaves, and reopens the manga
- **THEN** the reader opens with zoom `1:1`

#### Scenario: Navigation between chapters keeps chrome
- **WHEN** the user sets zoom `fit-w` and mode `spread-h`, then crosses into the next chapter
- **THEN** the new chapter opens with zoom `fit-w` and mode `spread-h`

#### Scenario: Absent zoom keeps stored value
- **WHEN** the client omits `zoom` in POST /reader/position
- **THEN** the stored zoom is preserved

### Requirement: Keyboard navigation preserved
Arrow-key page navigation SHALL remain functional with nav hidden or shown, excluding keys targeted at form controls.

#### Scenario: Arrow key pages while hidden
- **WHEN** nav is hidden and the user presses ArrowRight (respecting RTL)
- **THEN** the reader advances one page
