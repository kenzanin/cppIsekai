# Spec Delta

## ADDED Requirements

### Requirement: Fresh search served from cache
For opt-in methods (`search` first), the host SHALL consult the `search_cache` read model
before invoking the plugin: a row whose `fetched_at` is within `ttl_s` SHALL be returned
directly from the database without any plugin execution or outbound request, and the
response shape SHALL be identical to the plugin call it replaces.

#### Scenario: Repeat query hits the database
- **WHEN** the same search is requested twice within the TTL
- **THEN** the second response is served from `search_cache` and no plugin call occurs

### Requirement: Expired entry refreshes via plugin
When no row exists or the stored row is older than its TTL, the host SHALL call the
plugin as usual and write the successful result back (upsert) with a fresh `fetched_at`.

#### Scenario: Expired entry triggers refresh
- **WHEN** a cached result is past `ttl_s` and the same search is requested
- **THEN** the plugin runs, the response is returned, and the row is replaced

### Requirement: Canonical query identity
The cache key SHALL be a hash over the method parameters in canonical form (sorted keys,
normalized types) together with the plugin id, so equivalent queries with different
key order share one entry and distinct queries never collide.

#### Scenario: Key order does not split the cache
- **WHEN** the same search is issued with JSON keys in different orders
- **THEN** both resolve to the same `query_hash` and the second is a cache hit
