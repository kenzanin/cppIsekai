# Spec Delta: chapter-store

## MODIFIED Requirements

### Requirement: Chapter identity rows materialize on parse

Chapter identity rows (`chapters`, four-part PK with `plugin`/`slug`) SHALL be persisted only for pairs
that have a `saved_manga` row. For an unsaved pair the same parsed identities, `flags`, `read_max` and
page URL lists SHALL be held in process memory until the pair is saved (promotion) or the process
ends. Parsing SHALL keep the plugin-emitted `number`, `title`, `url` and `release_at` (verbatim), SHALL
default `flags` to 0, and SHALL stamp `updated_at` itself. Rows SHALL NOT be deleted
when flags return to 0. The unchanged-seed short circuit SHALL include `release_at`.

#### Scenario: Unsaved pair keeps rows in memory
- **WHEN** a chapter list is parsed for a pair with no saved row
- **THEN** no `chapters` row is written and the reader still receives the list and per-chapter flags

#### Scenario: Saved pair persists as before
- **WHEN** a chapter list is parsed for a saved pair with 30 chapters
- **THEN** 30 rows exist keyed by that manga with `flags = 0` and the emitted `release_at`

#### Scenario: Promotion writes the memory rows
- **WHEN** an unsaved pair with parsed chapters is saved
- **THEN** those identities and their flags/read_max become `chapters` rows in the save transaction

#### Scenario: First open materializes chapters
- **WHEN** a chapter-list parse succeeds for a saved manga with 30 chapters
- **THEN** 30 rows exist keyed by that manga with distinct `chapter_id`s and `flags = 0`

#### Scenario: Re-parse refreshes identity, keeps flags
- **WHEN** the list is re-fetched and a chapter's title changed
- **THEN** the existing row updates `title`/`number`/`url` and keeps its `flags` value

#### Scenario: Release date only change still refreshes
- **WHEN** a re-parse differs from the stored seed set only by one chapter's `release_at`
- **THEN** the row's `release_at` is updated instead of the parse being skipped

## ADDED Requirements

### Requirement: The chapter list is served from stored rows

`GET /api/chapters/<plugin>/<slug>` SHALL answer from the `chapters` rows when at least one row
exists for the pair, without calling the plugin: the response SHALL be the same envelope shape a
plugin parse produces — `{"ok":true,"cached":true,"data":[{id, number, title, url, release_at}]}` —
ordered by chapter number ascending. When no row exists the current cached-or-live path SHALL apply
unchanged. The stored rows SHALL be refreshed by a parse before the response is built.

#### Scenario: Saved title answers locally
- **WHEN** a manga has `chapters` rows and `GET /api/chapters` is requested with the plugin unreachable and the `api_cache` row pruned
- **THEN** the endpoint returns the stored list with `cached:true` and no plugin call occurs

#### Scenario: Cold miss falls through to the plugin
- **WHEN** no `chapters` row exists for the pair
- **THEN** the request goes through the existing cached-or-live plugin path and the rows are materialized from the result

#### Scenario: Stored order is reading order
- **WHEN** the stored rows are served
- **THEN** entries are ordered by `number` ascending and `release_at` is present exactly as parsed
