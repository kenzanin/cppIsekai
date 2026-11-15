# Spec Delta: library-save

## MODIFIED Requirements

### Requirement: Saved entries are listed
`GET /api/library` SHALL return the saved entries (name, cover, plugin, slug, and the stat fields `status, total_ch, read_ch, last_read_at, updated_at, status_flags`) ordered by `added_at` descending. The `has_update` stat field SHALL NOT be returned; the New state is carried by `status_flags` bit0. The home/Library grid SHALL render saved entries as cover cards linking to `/view/manga/<plugin>/<slug>`; an empty library SHALL show the empty state instead of the search-cache grid. In saved mode the grid is client-paginated (12/page) instead of server-paginated. The response SHALL also carry the stored display-detail fields (`authors`, `artists`, `tags`).

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

#### Scenario: Detail fields travel with the listing
- **WHEN** `GET /api/library` returns a saved entry that has stored detail
- **THEN** its `authors`, `artists` and `tags` are present in the same object as its name and cover

## ADDED Requirements

### Requirement: A saved entry carries its display detail
`saved_manga` SHALL store the detail the detail page renders: `name`, `cover`, `status`, `authors`, `artists`, `tags`, and a merged `synopsis_display`. Library cards, the detail heading and the detail API SHALL take a saved manga's display values from this row. The plugin-source `synopsis` and `alt_titles` columns SHALL keep their current meaning and remain the input of duplicate clustering.

#### Scenario: Card and heading agree
- **WHEN** the library grid and the detail page render the same saved manga
- **THEN** both show the name stored on the row

#### Scenario: Unsaved titles have no row
- **WHEN** a title is not in the library
- **THEN** no display detail is stored for it and its metadata stays live from the plugin

#### Scenario: Source synopsis is untouched
- **WHEN** a detail open writes `synopsis` and `alt_titles` from the plugin envelope
- **THEN** the merged display synopsis in `synopsis_display` is unchanged, and duplicate grouping is computed from the same values as before the change

### Requirement: The route materializes display detail
When the detail API serves a resolved envelope for a saved manga and the resolved values differ from the stored ones, the host SHALL write them to the row and SHALL skip the write when they are unchanged. Fetching enrichment candidates or saving `chosen` SHALL NOT write these columns; the merge itself SHALL stay side-effect free so enrichment callers keep seeing an untouched `saved_manga`.

#### Scenario: Resolve stores the detail
- **WHEN** a saved manga's detail page opens and the resolver yields a name, synopsis, authors, artists and tags
- **THEN** those values are stored on its `saved_manga` row

#### Scenario: Candidate fetch does not touch the row
- **WHEN** an enrichment source is fetched or `chosen` is saved
- **THEN** no display-detail column on `saved_manga` changes

#### Scenario: Unchanged resolve is read-only
- **WHEN** a saved manga's detail is served and the resolved values equal the stored ones
- **THEN** the row is not written

### Requirement: Reset clears materialized detail
`POST /api/enrichment/reset` SHALL clear the materialized display fields it invalidates (`detail` clears the merged title/synopsis/authors/artists/tags, `cover` clears the chosen cover) while keeping `title_meta.fetched`, so the next detail open re-resolves them from the plugin envelope.

#### Scenario: Reset detail re-resolves
- **WHEN** the user resets detail on a saved manga and reopens the page
- **THEN** the materialized fields are repopulated from the scraper envelope instead of showing the reset values

#### Scenario: Reset keeps candidates
- **WHEN** the user resets cover or detail
- **THEN** `title_meta.fetched` is unchanged and the picker still lists the fetched candidates

### Requirement: The library upsert never stores a placeholder name
`POST /api/library` SHALL skip writing `name` when the submitted value is empty or equal to the slug, and SHALL NOT overwrite a non-empty stored name with such a value.

#### Scenario: Plugin unreachable on re-add
- **WHEN** a saved manga without stored detail is re-added while its plugin is unreachable and the page submits the slug as the name
- **THEN** the stored name is left as-is
