# Spec Delta: library-save

## ADDED Requirements

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
rows, the pair's `api_cache` envelope rows, and the pair's disk image tree. Legacy chapter-id-keyed
`host/skip|read|total` shadows swept with the pair's chapter ids; re-adding the pair SHALL equal
adding a never-opened title.

#### Scenario: Remove leaves no pair data
- **WHEN** a saved manga with chapters, reader position, enrichment chosen and cached envelopes is removed
- **THEN** none of those rows remain for the pair and its image directory is gone

#### Scenario: Neighbours survive
- **WHEN** the pair is removed
- **THEN** other slugs of the same plugin and the same slug under other plugins keep their rows and images

#### Scenario: Re-add equals a fresh add
- **WHEN** the pair is added again after the remove
- **THEN** its row carries no display detail and the next detail open pays a cold cache miss
