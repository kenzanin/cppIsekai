# Spec Delta: manga-detail-page

## MODIFIED Requirements

### Requirement: Stale-serve on detail and chapters data
`GET /api/detail/<plugin>/<slug>` and `GET /api/chapters/<plugin>/<slug>` SHALL serve locally held data with `cached:true` when the plugin call is not made or fails, SHALL 404 with `{"ok":false,"error":"unknown title"}` on invalid slug/plugin pairs, and SHALL never crash the host on plugin errors. Local-first order: a saved row with materialized detail answers `/api/detail`; `chapters` rows answer `/api/chapters`; `api_cache` and the live plugin call are the fallbacks behind both. A saved manga whose rows exist SHALL therefore answer both endpoints without any plugin call, regardless of `api_cache` age or the cache-prune schedule.

#### Scenario: Cached serve when upstream fails
- **WHEN** the plugin call fails but a previous response is cached
- **THEN** the endpoint returns the cached envelope plus `cached:true`

#### Scenario: Saved title survives cache pruning
- **WHEN** a saved manga's `api_cache` rows have been pruned and the plugin is unreachable
- **THEN** `/api/detail` answers from the saved row and `/api/chapters` from the `chapters` rows, both with `cached:true`

#### Scenario: Unsaved title still needs the cache or the plugin
- **WHEN** an unsaved manga has no `chapters` rows and no `api_cache` row
- **THEN** the plugin call runs and its failure envelope is returned

#### Scenario: Unknown title guard
- **WHEN** the slug or plugin does not resolve in the store
- **THEN** the endpoint returns 404 with the unknown-title error body
