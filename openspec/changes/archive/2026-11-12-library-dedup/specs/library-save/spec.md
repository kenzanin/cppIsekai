## ADDED Requirements

### Requirement: Saved entries store duplicate signal
Saved entries SHALL store the source plugin's `synopsis` and `alt_titles` (JSON array, verbatim) when present in the detail envelope; they are refreshed on save and on detail refresh; enrichment values are NOT used for this.

#### Scenario: Synopsis stored on refresh
- **WHEN** refresh returns `detail.synopsis`
- **THEN** the saved row stores it

### Requirement: FTS5 duplicate grouping
The store SHALL maintain an FTS5 index over saved-entry `name`, `alt_titles`, `synopsis`; on save/unsave/refresh it SHALL recompute `dup_group` (0 = unique) via FTS5 candidate generation + token-Dice similarity ≥ 0.6 in connected components. If FTS5 is unavailable, `dup_group` stays 0 and the duplicate filter reports no duplicates (no crash).

#### Scenario: Cross-plugin duplicates grouped
- **WHEN** two saved entries from different plugins share normalized alt-title tokens and similar synopsis
- **THEN** both rows get the same non-zero `dup_group`

#### Scenario: Identical title same site grouped
- **WHEN** the same entry is saved unsubscribe/re-saved (same plugin, same slug) it is one row — n/a; duplicate means different slug/plugin with same work

### Requirement: Duplicate filter clusters adjacently
In the Library grid, activating the "duplicate" chip SHALL show only rows with non-zero `dup_group` and SHALL render each cluster contiguously (client-side sort by `dup_group`, then name).

#### Scenario: Adjacent duplicates
- **WHEN** three cluster members exist among other unique entries
- **THEN** the three render consecutively under the duplicate filter

### Requirement: Grid exposes dup marker
Each saved card SHALL carry `data-dup=<dup_group>` so the grid can filter/sort without extra requests.

#### Scenario: Unique entries hidden under filter
- **WHEN** the chip is active and only 2 of 50 saved rows have non-zero `dup_group`
- **THEN** exactly those 2 render
