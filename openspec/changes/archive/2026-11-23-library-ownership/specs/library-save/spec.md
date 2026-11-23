# Spec Delta: library-save

## ADDED Requirements

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
