# search-cache Specification

## Purpose

Plugin API responses read model in sqlite as JSONB binary: repeat calls are served from `api_cache` without touching the plugin; expiry refreshes via the plugin; failure serves the last-good row with `cached:true`. Replaces the `data/cache/api/` stale file pool.

## Requirements

### Requirement: Plugin API responses are served from a JSONB read model
For every stale-opt-in method (`search`, `detail`, `chapters`, `pages`, `genres`), the host SHALL consult the `api_cache` read model in sqlite **before** invoking the plugin: a row whose `fetched_at` is within `ttl_s` SHALL be returned directly from the database without any plugin execution or outbound request, and the response shape SHALL be identical to the plugin call it replaces (the stored envelope's `data` is returned verbatim, `cached` flag absent). The `payload` column SHALL be written with `jsonb(?)` and read with `json(payload)` — JSONB binary, not text JSON.

#### Scenario: Repeat call hits the database
- **WHEN** a stale-opt-in method is requested twice within TTL
- **THEN** the second response is served from `api_cache` and no plugin call occurs

### Requirement: Expired entry refreshes via the plugin
When no row exists or the stored row is older than TTL, the host SHALL call the plugin as usual and, on success, write the result back (upsert) with a fresh `fetched_at`. On plugin failure, the last-good row SHALL be served with `cached:true` merged at the top level; if no row exists, the plugin's failure envelope is returned verbatim.

#### Scenario: Expired entry triggers refresh
- **WHEN** a cached result is past `ttl_s` and the same request is issued
- **THEN** the plugin runs, its response is returned, and the row is replaced with a fresh `fetched_at`

#### Scenario: Plugin failure serves last-good with cached flag
- **WHEN** the plugin call fails and a stale row exists
- **THEN** the stored envelope is returned with top-level `cached:true` merged via the same glaze-merge path as before

#### Scenario: Plugin failure with no row
- **WHEN** the plugin call fails and no row exists
- **THEN** the plugin's failure envelope is returned unchanged

### Requirement: Canonical query identity
The cache key SHALL be a hash over the method parameters in canonical form (sorted keys, dropped empties) together with the plugin id and method, so equivalent queries with different key order share one entry and distinct queries never collide.

#### Scenario: Key order does not split the cache
- **WHEN** the same request is issued with JSON keys in different orders
- **THEN** both resolve to the same `query_hash` and the second is a cache hit

### Requirement: The api file pool is removed
The stale file cache pool `data/cache/api/` SHALL be removed: the cache root contract becomes img-only, the boot scan / legacy-purge / LRU accounting SHALL NOT reference an `api` subpool, and all producers/consumers of `Cache::store/peek(..., "api", ...)` SHALL use the store JSONB accessors instead. Existing `.bin` files are dropped, not migrated.

#### Scenario: Boot on an img-only root
- **WHEN** the daemon boots with no `data/cache/api/` directory
- **THEN** it starts normally, and a stale-opt-in plugin call writes only to `api_cache`

#### Scenario: No code path writes the api file pool
- **WHEN** a stale-opt-in method succeeds
- **THEN** no file is created under `data/cache/api/`
