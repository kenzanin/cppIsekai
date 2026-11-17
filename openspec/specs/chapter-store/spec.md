# chapter-store Specification

## Purpose
TBD - created by archiving change chapter-state. Update Purpose after archive.

## Requirements

### Requirement: Chapter identity rows materialize on parse

The store SHALL maintain one `chapters` row per `(plugin, slug, chapter_id)`, upserted from
every successful chapter-list parse with the plugin-emitted `number`, `title`, and source `url`;
`flags` SHALL default to 0 and `updated_at` SHALL be stamped by the store. Rows SHALL NOT be
deleted when flags return to 0.

#### Scenario: First open materializes chapters

- **WHEN** a chapter-list parse succeeds for a manga with 30 chapters
- **THEN** 30 rows exist keyed by that manga with distinct `chapter_id`s and `flags = 0`

#### Scenario: Re-parse refreshes identity, keeps flags

- **WHEN** the list is re-fetched and a chapter's title changed
- **THEN** the existing row updates `title`/`number`/`url` and keeps its `flags` value

### Requirement: Chapter flags bitfield

The `chapters.flags` column SHALL be a bitfield: `bit0 = read`, `bit1 = hidden`, higher bits
reserved. All flag writes SHALL occur inside a single store operation scope.

#### Scenario: Toggle sets exactly one bit

- **WHEN** bit0 is set on a row with `flags = 0`
- **THEN** the row reads `flags = 1` and bit1 remains clear

### Requirement: Page URL rows materialize on pages parse

The store SHALL maintain `chapter_pages(plugin, slug, chapter_id, idx, url)` rows, replaced
wholesale on every successful pages parse for that chapter, `idx` being the 0-based reading
order. The image cache filesystem SHALL remain the source of truth for cached files; no file
paths SHALL be stored.

#### Scenario: Pages parse replaces children

- **WHEN** a pages parse yields 14 URLs for a chapter that had 12 stored
- **THEN** the chapter has exactly 14 page rows in parse order and no stale rows

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
