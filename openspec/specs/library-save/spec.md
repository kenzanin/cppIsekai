# library-save Specification

## Purpose

Save manga to a personal library: a `saved_manga` store table, JSON routes to save/unsave/list, a Library grid on the home page rendering saved entries as cover cards, and a client-side toggle on the detail page hydrated from `GET /api/library` membership.

## Requirements

### Requirement: Save and unsave a manga
`POST /api/library` SHALL upsert a `saved_manga` row `{plugin, slug, name, cover, added_at}` (unique on `(plugin, slug)`); `POST /api/library/remove` SHALL delete it. The response SHALL report the resulting state (`saved: true|false`).

#### Scenario: Add to library toggles on
- **WHEN** the user clicks "Add to library" on a detail page that is not saved
- **THEN** the entry is persisted and the button becomes "In library"

#### Scenario: Re-add is idempotent
- **WHEN** `POST /api/library` targets an already-saved entry
- **THEN** the row is updated (name/cover refreshed) without duplicating rows

#### Scenario: Remove from library toggles off
- **WHEN** the user clicks "In library" on a saved entry
- **THEN** the entry is deleted and the button becomes "Add to library"

### Requirement: Saved entries are listed
`GET /api/library` SHALL return the saved entries (name, cover, plugin, slug, and the stat fields `status, total_ch, read_ch, last_read_at, updated_at, status_flags`) ordered by `added_at` descending. The `has_update` stat field SHALL NOT be returned; the New state is carried by `status_flags` bit0. The home/Library grid SHALL render saved entries as cover cards linking to `/view/manga/<plugin>/<slug>`; an empty library SHALL show the empty state instead of the search-cache grid. In saved mode the grid is client-paginated (12/page) instead of server-paginated.

`status_flags` SHALL be the single per-manga state integer on `saved_manga`, with this layout:

- **bit0 (1)** — is_new: a refresh observed chapter growth beyond the stored `total_ch`. Cleared by a reader touch (`saved_clear_update`).
- **bit1 (2)** — is_done: `total_ch > 0` and `read_ch >= total_ch`.
- **bit2 (4)** — category *ongoing*: source `status` folds to `"ongoing"`.
- **bit3 (8)** — category *hiatus*: source `status` folds to `"hiatus"`.
- **bit4 (16)** — category *canceled*: source `status` folds to `"canceled"` or `"cancelled"`.
- **bit5 (32)** — category *completed*: source `status` folds to `"completed"`.
- **bit6 (64)** — is_dim: the user toggled dim+blur on this entry.

The source `status` string SHALL still be stored verbatim on the row and remains the display source (detail-page chip); the category bits are its derived, queryable classification. An unrecognized status string sets no category bit and does not error. Every writer of the stat fields SHALL preserve bit6; only the dim toggle sets or clears it.

The grid's "Complete" filter SHALL gate on bit5 (`status_flags & 32`) instead of string-matching the status text. The detail page SHALL render category badges (Ongoing/Hiatus/Canceled/Complete) gated on bits 2–5, alongside the existing New (bit0) and Done (bit1) badges.

#### Scenario: Saved entry appears on home
- **WHEN** a manga is saved and the home page is opened
- **THEN** its cover card appears with its stat fields in the SSR context

#### Scenario: Unsaved entry disappears
- **WHEN** a saved entry is unsaved and the home page reloads
- **THEN** its card is gone

#### Scenario: Library vs search cache
- **WHEN** the `title` search-cache table contains seed rows but nothing is saved
- **THEN** the Library grid renders the empty state, not the search cache

#### Scenario: Saved view ignores search filters (2026-11-10)
- **WHEN** the request carries `?plugin=`, `?q=`, or `?page=` and saved entries exist
- **THEN** the grid renders the saved entries regardless of the filter values (client-side controls may then apply `?plugin=` as the initial chip)

#### Scenario: Deep-link plugin filter
- **WHEN** the home page is opened with `?plugin=<id>` and saved entries exist
- **THEN** the grid pre-filters to that plugin client-side

#### Scenario: Category bit matches the source status
- **WHEN** a saved entry's source status is `"Hiatus"` (or any case variant of a known category)
- **THEN** exactly the matching category bit is set in `status_flags` (bit3 for hiatus) and the detail page renders the corresponding badge

#### Scenario: Unknown status sets no category bit
- **WHEN** a saved entry's source status is a string outside the known categories
- **THEN** no category bit is set, the raw status text is still shown on the detail page, and no error occurs

#### Scenario: Complete filter uses the stored bit
- **WHEN** the user activates the "Complete" grid filter
- **THEN** only cards whose `data-status-flags` has bit5 set are shown (no string comparison of `data-status`)

#### Scenario: Stat writers preserve the dim bit
- **WHEN** a stats refresh or reader rollout updates a dimmed entry
- **THEN** bit6 stays set and the entry remains dimmed

#### Scenario: New state survives without has_update
- **WHEN** `GET /api/library` is called for an entry whose chapter list grew
- **THEN** the response carries `status_flags` with bit0 set and carries no `has_update` field

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

### Requirement: Toggle state is hydrated client-side
The detail page SHALL initialize its toggle from `GET /api/library` membership (plugin+slug) at boot, mirroring the existing `loadPluginMeta()` pattern; it SHALL NOT rely on a server-side `saved` field on the detail response.

#### Scenario: Saved manga reloaded
- **WHEN** the detail page for a saved manga is loaded
- **THEN** the button renders "In library" on first paint (after the membership fetch resolves before any user interaction)

#### Scenario: Toggle failure
- **WHEN** `POST /api/library` or `POST /api/library/remove` fails
- **THEN** the error surfaces on the detail page's existing `err` line and the button label stays consistent with the last known state

### Requirement: Per-manga dim toggle is stored in status_flags
The dim/blur cover toggle SHALL be persisted as `status_flags` bit6 on the saved entry, replacing the `host/dim/<plugin>/<slug>` `plugin_state` key. `POST /api/display/dim-blur` SHALL write it through `store.saved_dim_update(plugin, slug, on)`, which touches only bit6; `GET /api/display/dim-list` SHALL scan saved rows for that bit. A toggle for an unsaved manga SHALL be a no-op.

#### Scenario: Dim state persists for a saved manga
- **WHEN** the user toggles dim+blur on a saved manga
- **THEN** bit6 is set on that row and the dim state survives a reload, read from `status_flags`

#### Scenario: Dim toggle is reversible
- **WHEN** the user toggles dim+blur off for a dimmed saved manga
- **THEN** bit6 is cleared and the other flag bits are unchanged

#### Scenario: dim-list reflects stored bits
- **WHEN** `GET /api/display/dim-list` is called
- **THEN** it returns exactly the saved entries whose `status_flags` has bit6 set

#### Scenario: Unsaved manga cannot be dimmed
- **WHEN** the user toggles dim+blur on a manga that is not in the library
- **THEN** the request succeeds without persisting state and no row is created
