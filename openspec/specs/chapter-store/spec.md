# chapter-store Specification

## Purpose
TBD - created by archiving change chapter-state. Update Purpose after archive.

## Requirements

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

### Requirement: Chapter flags bitfield

The `chapters.flags` column SHALL be a bitfield: `bit0 = read`, `bit1 = hidden`, higher bits
reserved. All flag writes SHALL occur inside a single store operation scope.

#### Scenario: Toggle sets exactly one bit

- **WHEN** bit0 is set on a row with `flags = 0`
- **THEN** the row reads `flags = 1` and bit1 remains clear

### Requirement: Page URL rows materialize on pages parse
The store SHALL maintain `chapter_pages(plugin, slug, chapter_id, idx, url)` rows, replaced
wholesale on every successful pages parse for that chapter, `idx` being the 0-based reading order.
The rows SHALL be the durable page list: a chapter with rows SHALL NOT require a plugin call to be
read, and a background revalidation SHALL replace them with the same wholesale semantics. The image
cache filesystem SHALL remain the source of truth for cached files; no file paths SHALL be stored.

#### Scenario: Pages parse replaces children
- **WHEN** a pages parse yields 14 URLs for a chapter that had 12 stored
- **THEN** the chapter has exactly 14 page rows in parse order and no stale rows

#### Scenario: Rows survive the envelope cache
- **WHEN** the pair's `api_cache` rows are pruned and the chapter is opened again
- **THEN** the stored page rows still answer the request in their parse order

#### Scenario: Revalidation replaces the list
- **WHEN** a background revalidation returns a different URL list for the chapter
- **THEN** the rows hold only the new list, and the cached image files are untouched

### Requirement: Bulk flags API

`POST /chapter/flags` SHALL accept `{plugin, slug, ids? | all?, set?, clear?}` and apply
`(flags | set) & ~clear` to the selected rows in one transaction, returning per-id resulting
flags. A request with neither `ids` nor `all` SHALL be a 400.

#### Scenario: Mark a range read

- **WHEN** the client posts `ids` of chapters 1..10 with `set: 1`
- **THEN** exactly those rows carry bit0 and the response lists `flags = 1` for each

#### Scenario: Clear all

- **WHEN** the client posts `all: true, clear: 1`
- **THEN** bit0 is cleared on every chapter row of the manga and bit1 is untouched

### Requirement: Flags read helper

The store SHALL expose per-id and per-manga flag readers; the reader window skip filter, the
`/chapter/state` route, and the `saved_manga.read_ch` rollup SHALL consume these instead of
`plugin_state` keys.

#### Scenario: Reader honors stored skip bit

- **WHEN** a chapter has bit1 set and the window is composed
- **THEN** the window excludes that chapter

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
