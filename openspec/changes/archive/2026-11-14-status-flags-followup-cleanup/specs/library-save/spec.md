# Spec Delta: library-save

## MODIFIED Requirements

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

## ADDED Requirements

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
