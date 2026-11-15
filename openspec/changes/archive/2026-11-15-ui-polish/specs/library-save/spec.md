# Spec Delta: library-save

## ADDED Requirements

### Requirement: Grid exposes the status category filter
The home/Library grid SHALL offer filter controls for the stored status categories (ongoing,
hiatus, canceled, completed) alongside the existing New/Done/Complete controls. Each control
SHALL filter on the corresponding `status_flags` category bit, and the grid SHALL NOT
recompute a category from the `status` string or from chapter counts.

#### Scenario: Filter by an ongoing title
- **WHEN** the user selects the ongoing filter and a saved entry carries the ongoing category bit
- **THEN** that card is shown

#### Scenario: Category filter excludes other categories
- **WHEN** the user selects the ongoing filter and a saved entry carries the completed bit
- **THEN** that card is hidden

#### Scenario: One page reset for all controls
- **WHEN** the user changes any category control while on a page other than the first
- **THEN** the grid returns to the first page, and the New/Done/Complete controls behave the same way

### Requirement: Unrecognised status renders a neutral chip
A saved entry whose `status_flags` category bits are all clear SHALL render a neutral chip
carrying the raw `status` string, rather than no chip at all.

#### Scenario: Unrecognised status stays visible
- **WHEN** a saved entry's `status` string folds to no category bit
- **THEN** the card renders a neutral chip showing the raw `status`, and no category control matches it

### Requirement: Dim is toggleable from the library grid
Each saved entry in the grid SHALL expose a control that toggles the per-entry dim state.
The rendered dim state SHALL be derived from `status_flags` bit6 as returned by the server
after the toggle, never applied optimistically before the response.

#### Scenario: Toggle dim from the grid
- **WHEN** the user toggles dim on a saved grid card
- **THEN** the entry is persisted as dim and the card renders dimmed from the returned `status_flags`

#### Scenario: Unsaved entry has no dim state
- **WHEN** a card does not correspond to a saved entry
- **THEN** no dim control is offered, because the bit lives on the saved row
