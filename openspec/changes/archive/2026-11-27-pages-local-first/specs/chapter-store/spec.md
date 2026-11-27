# Spec Delta: chapter-store

## MODIFIED Requirements

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
