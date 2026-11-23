# Spec Delta: chapter-store

## MODIFIED Requirements

### Requirement: Chapter identity rows materialize on parse

The store SHALL maintain one `chapters` row per `(plugin, slug, chapter_id)`, upserted from every
successful chapter-list parse with the plugin-emitted `number`, `title`, source `url`, and
`release_at` (stored verbatim: epoch seconds, ISO date text, or empty when the envelope omits it);
`flags` SHALL default to 0 and `updated_at` SHALL be stamped by the store. Rows SHALL NOT be deleted
when flags return to 0. The unchanged-seed short circuit SHALL include `release_at`, so a parse whose
only change is a release date still rewrites the rows.

#### Scenario: First open materializes chapters

- **WHEN** a chapter-list parse succeeds for a manga with 30 chapters
- **THEN** 30 rows exist keyed by that manga with distinct `chapter_id`s, `flags = 0`, and the emitted `release_at`

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
