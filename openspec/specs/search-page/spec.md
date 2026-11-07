# search-page Specification

## Purpose

Defines the host-rendered search page: plugin-gated search form, genre filtering, paginated results with totals, URL-encoded view state, and inline loading/error states.

## Requirements

### Requirement: Plugin-gated search form
The page SHALL offer exactly one plugin selector listing ENABLED plugins (logo with
initial fallback) and a text query with an explicit Search action. A disabled or missing
selection SHALL show a notice and disable the search controls.

#### Scenario: Disabled plugin not selectable
- **WHEN** mangadex is disabled and the page loads
- **THEN** the picker offers no mangadex and the search button is inert with a notice

### Requirement: Genre filter row
When the selected plugin answers `genres`, the page SHALL render its values as toggleable
chips; selected chips AND-filter the results via the search `tags` param. The row SHALL be
hidden when the call fails or the plugin lacks the method.

#### Scenario: Chips narrow results
- **WHEN** `Isekai` and `Fantasy` chips are active for a mangadex search
- **THEN** the request carries both and every returned item matches both

#### Scenario: Plugin without genres
- **WHEN** a plugin returns an error for `genres`
- **THEN** no genre row renders and search works without it

### Requirement: Paged results with count
Results SHALL show the total count, a cover grid (hue fallback placeholder), and Prev/Next
pagination above AND below the grid; page size 12; bounds disable the buttons; `page` is
1-based and clamped by the plugin.

#### Scenario: Pagination walks pages
- **WHEN** a query returns total 30
- **THEN** pages=3, page 1 shows items 1-12, Next yields 13-24, Prev returns

#### Scenario: No results
- **WHEN** the query matches nothing
- **THEN** count shows 0 with an empty-state line, grid hidden

### Requirement: URL-encoded state
`plugin`, `q`, `tags`, `page` SHALL round-trip through the URL; load SHALL restore the
exact result view; actions SHALL pushState.

#### Scenario: Shareable result
- **WHEN** a results URL is opened fresh
- **THEN** the same plugin/query/tags/page results render without extra clicks

### Requirement: Inline states
Loading SHALL show a skeleton grid; plugin `ok:false` responses SHALL render the error
message with a retry above the grid; no alert/dialog.

#### Scenario: Upstream failure
- **WHEN** the plugin returns `{"ok":false,"error":"SSL connect error"}`
- **THEN** the message and a Retry control appear; the previous results stay visible until retry succeeds
