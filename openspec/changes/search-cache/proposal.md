# Proposal

## Why

Re-viewing search results re-invokes the plugin (HTTP to the site) every time, even though the response is deterministic given the query. OKF decision `decisions/search-read-model-cache` (user approved 2026-11-08) stores results as a JSONB read model in sqlite through `call_cached`, aligning with the jsonb probe in `normalization-contract`.

## What Changes

- Table `search_cache(plugin_id, query_hash PK, params_json, results JSONB, fetched_at, ttl_s)` in the store (the only SQL layer).
- Look-aside in **`call_cached`**: opt-in per method (`search` first) — check fresh cache before invoking the plugin; write-through upsert on success. Constant per-method TTL (ponytail: knob later).
- Key = sha1(query + canonicalized opts) — key order does not affect hits.
- The existing stale file cache REMAINS (offline fallback); the DB is the structured read model.
- Out of scope: other methods (pattern later), manual invalidation, UI.

## Capabilities

### New Capabilities

- `search-cache`: sqlite (JSONB) read model for search results with fresh-serve / expiry-refresh via `call_cached`.

### Modified Capabilities

(none)

## Impact

- Files: `src/store/store.{hpp,cpp}` (migration + accessors), `src/http/http.cpp` (`call_cached`), `src/plugin/plugin.cpp` (key helper — or in store), `tests/test_store.cpp`/`test_plugin.cpp`.
- Risk: double write path (DB + file cache) — accepted; prune later if proven redundant.
