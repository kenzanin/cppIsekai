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

### Requirement: Browse data is ephemeral

Data produced for a pair with no `saved_manga` row SHALL be held in process memory, not in the
database: chapter identities with flags/read_max, page URLs, reader position, enrichment rows and the
per-pair host fields. A daemon restart SHALL discard it. Store methods SHALL keep their signatures; the
routing between memory and SQLite is internal.

#### Scenario: Browsing writes nothing to SQLite
- **WHEN** a title from search is opened, read and left unsaved
- **THEN** no `chapters`, `chapter_pages`, `reader_positions` or `title_meta` row exists for the pair, while the reader and the detail page work normally

#### Scenario: Restart clears browse state
- **WHEN** the daemon restarts after browsing an unsaved title
- **THEN** that title starts with no flags, no progress and no position

#### Scenario: Saved pairs are unaffected
- **WHEN** a saved pair is read after a restart
- **THEN** its flags, read_max and position are the ones stored before the restart

### Requirement: Saving promotes the browse state

`POST /api/library` SHALL move the pair's ephemeral state into SQLite in one transaction: chapter
identities with their `flags` and `read_max`, page URL rows, reader position, enrichment rows and the
per-pair host fields, then release the memory entry. Chapter reads produced before saving SHALL survive.

#### Scenario: Read five chapters then save
- **WHEN** the user reads five chapters of an unsaved title and then adds it to the library
- **THEN** those five chapters carry their read marks and page progress into the saved rows and the library shows the matching read count

#### Scenario: Promotion needs no network
- **WHEN** the plugin is unreachable at the moment of saving
- **THEN** promotion still completes from memory and cached envelopes

### Requirement: Removed plugins retain their library entries

When a scan finds a registered plugin id with no folder, the host SHALL delete only that plugin's
caches and registry row (`title`, `api_cache`, `plugin_state` for the id, then the `plugin` row). Every
`saved_manga` row and its children SHALL be retained. The retained entries SHALL be reported as
`orphan` and SHALL serve from local rows without any plugin call.

#### Scenario: Plugin folder deleted
- **WHEN** a plugin folder is removed and the library is next scanned
- **THEN** the plugin leaves the registry while its saved titles, chapter rows and positions remain

#### Scenario: Orphan behaves like offline
- **WHEN** an orphan entry's detail page is opened while the plugin is absent
- **THEN** the page renders from local rows with the cached badge and makes no plugin call

#### Scenario: Reinstall revives entries
- **WHEN** a plugin with the same id is installed again
- **THEN** its retained entries are live again with no migration and no write to the library rows

### Requirement: Orphan entries are visible, migratable and purgeable

`GET /api/library` SHALL report each entry's upstream state (live, offline, orphan). The grid SHALL
render an orphan badge and offer an orphan filter. `POST /api/library/purge-orphans` SHALL delete every
orphan pair through the existing remove path (rows, cached envelopes, disk images).

#### Scenario: Orphan badge and filter
- **WHEN** the library contains an entry whose plugin is absent
- **THEN** its card shows the orphan badge and the orphan filter selects only such cards

#### Scenario: Purge orphans
- **WHEN** `POST /api/library/purge-orphans` runs
- **THEN** every orphan pair's rows, cached envelopes and images are gone and live entries are untouched

#### Scenario: Migrating an orphan
- **WHEN** the user migrates an orphan entry to a live plugin
- **THEN** the library row is rekeyed and the orphan state disappears

### Requirement: Adding a manga promotes its cached detail

`POST /api/library` SHALL promote the title's cached detail onto its saved row within the save's db
task: `name`, `cover`, `authors`, `artists`, `tags`, `synopsis_display` from the cached,
chosen-merged `detail` envelope. It SHALL reuse the existing materialization helpers, be idempotent
(unchanged values are not rewritten), and call no plugin. With no cached envelope it SHALL be a
no-op, leaving the lazy resolve-on-open path in place.

#### Scenario: Add right after opening the title
- **WHEN** a title's detail page was opened and the user then adds it to the library
- **THEN** its `saved_manga` row carries `authors`, `artists`, `tags` and `synopsis_display` before the next open

#### Scenario: Add with nothing cached
- **WHEN** `POST /api/library` runs for a pair with no `api_cache` row
- **THEN** the saved row is written with the submitted name/cover, the display detail stays empty, and the next detail open resolves and materializes it

#### Scenario: Promotion needs no network
- **WHEN** the plugin is disabled or unreachable at the moment of saving
- **THEN** the promotion still completes from local rows and no plugin call is attempted

#### Scenario: Re-add does not rewrite unchanged rows
- **WHEN** an already-promoted entry is saved again with identical cached data
- **THEN** no display-detail column is rewritten

### Requirement: Adding a manga promotes its chapters and stats

The same save SHALL promote the cached `chapters` envelope into chapter identity rows for the pair
and write the `status`/`total_ch` stats, so a promoted entry answers detail and chapter-list reads
from local rows only.

#### Scenario: Chapter rows exist right after add
- **WHEN** a title with a cached chapters envelope is added to the library
- **THEN** `chapters` rows exist for the pair without any plugin call

#### Scenario: Library open is fully local after add
- **WHEN** the user opens a promoted entry while the plugin is unreachable and the `api_cache` rows have been pruned
- **THEN** both `GET /api/detail` and `GET /api/chapters` answer from local rows with `cached:true`

### Requirement: Unsave wipes the pair

`POST /api/library/remove` SHALL delete, besides the `saved_manga` row: the pair's `chapters`,
`chapter_pages`, `reader_positions`, `update_attempts`, `title_meta` and pair-scoped `plugin_state`
rows, the pair's `api_cache` envelope rows, and the pair's disk image tree. Read progress lives on
the master row (`chapters.read_max`) -- the legacy host shadows are retired and boot-backfilled;
re-adding the pair SHALL equal adding a never-opened title.

#### Scenario: Remove leaves no pair data
- **WHEN** a saved manga with chapters, reader position, enrichment chosen and cached envelopes is removed
- **THEN** none of those rows remain for the pair and its image directory is gone

#### Scenario: Neighbours survive
- **WHEN** the pair is removed
- **THEN** other slugs of the same plugin and the same slug under other plugins keep their rows and images

#### Scenario: Re-add equals a fresh add
- **WHEN** the pair is added again after the remove
- **THEN** its row carries no display detail and the next detail open pays a cold cache miss
