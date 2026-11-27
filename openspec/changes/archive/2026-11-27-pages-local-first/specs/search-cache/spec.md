# Spec Delta: search-cache

## MODIFIED Requirements

### Requirement: Plugin API responses are served from a JSONB read model
For every stale-opt-in method (`search`, `detail`, `chapters`, `genres`, `related`), the host SHALL
consult the `api_cache` read model in sqlite **before** invoking the plugin: a row whose `fetched_at`
is within `ttl_s` SHALL be returned directly from the database without any plugin execution or
outbound request, and the response shape SHALL be identical to the plugin call it replaces (the
stored envelope's `data` is returned verbatim, `cached` flag absent). The `payload` column SHALL be
written with `jsonb(?)` and read with `json(payload)` — JSONB binary, not text JSON.

`pages` SHALL NOT be a stale-opt-in method: the page list's durable copy is the `chapter_pages` table
(`chapter-store`), and the reader route reads that instead of an envelope row. Consequently no
per-method TTL constant exists beyond the 24h default; a plugin manifest override stays per
plugin+method.

#### Scenario: Repeat call hits the database
- **WHEN** a stale-opt-in method is requested twice within TTL
- **THEN** the second response is served from `api_cache` and no plugin call occurs

#### Scenario: Pages calls no longer write an envelope row
- **WHEN** a plugin `pages` call succeeds through the debug route
- **THEN** no `api_cache` row is written for it, and existing page rows age out through the prune
  scheduler untouched
