# plugins-page delta

## ADDED Requirements

### Requirement: Plugin rows
The page SHALL list every `plugin` row with: logo (initial fallback when absent), name,
version, runtime badge, capability chips, title count, and a client-side search box
filtering by name or capability.

#### Scenario: Row renders
- **WHEN** the page loads with a mangadex row
- **THEN** the row shows logo (or initial), `MangaDex`, version, `lua` badge, its capability chips and title count

#### Scenario: Search filters rows
- **WHEN** the user types `manga` in the box
- **THEN** only rows whose name or capabilities match remain

### Requirement: Three-state status with last error
Each row SHALL show one of `active` (enabled, last call ok), `error` (enabled, last call
failed — the error message SHALL be visible), or `disabled`. Health comes from the host's
in-memory last-call outcome; a restart resets it to active until the next call.

#### Scenario: Failing plugin visible
- **WHEN** a plugin's latest call failed with `SSL connect error`
- **THEN** its row shows `error` and that message

### Requirement: Enable/disable persists and gates
The row toggle SHALL persist `plugin.enabled` and take effect immediately: disabled
plugins are greyed on the page, excluded from the search-page plugin picker and library
filters, and their ABI calls return `{"ok": false, "error": "plugin disabled"}`. The
runtime stays loaded; re-enable is instant.

#### Scenario: Disable then search
- **WHEN** mangadex is disabled and the user opens the search page
- **THEN** the plugin picker does not offer mangadex, and a direct call to it returns the disabled error shape

#### Scenario: Re-enable
- **WHEN** the toggle is set back on
- **THEN** calls succeed again without a host restart

### Requirement: Title count deep-link
The title count SHALL be a link to `/library?plugin=<id>`; the library page SHALL filter
to that plugin's titles.

#### Scenario: Count click
- **WHEN** the Flame row shows `12` and it is clicked
- **THEN** the library page lists exactly Flame's titles

### Requirement: Reload
Each row SHALL have a reload action that triggers the existing plugin rescan; the page
SHOULD reflect the new state without a manual reload when it completes. The host SHALL log
the rescan as today.

#### Scenario: Reload after edit
- **WHEN** a plugin's `main.lua` was edited and the row reload is pressed
- **THEN** the next call runs the new code

### Requirement: Empty state
With no plugin rows the page SHALL show guidance (plugins dir path + expected folder
layout) instead of an empty table.

#### Scenario: No plugins installed
- **WHEN** the plugin table is empty and the page loads
- **THEN** the guidance block renders instead of the table
