# Proposal

## Why

Re-viewing plugin responses re-invokes the plugin (HTTP to the source site) every time, even though the response is deterministic given the query. The stale file cache (`data/cache/api/*.bin`) is a sha1-keyed JSON pile with no TTL, no structure and no read model. OKF decision `decisions/search-read-model-cache` (user approved 2026-11-08) plus the 2026-11-10 direction: move the whole api read model into sqlite as **real JSONB** (`jsonb()` / `json(payload)`), gated behind the store layer — not text JSON, not raw SQL outside `src/store/`.

## What Changes

- Table `api_cache(plugin_id TEXT, query_hash TEXT, params_json TEXT, payload JSONB BLOB, fetched_at INTEGER, ttl_s INTEGER, PRIMARY KEY(plugin_id, query_hash))` in the store — JSONB binary, accessed only through new `Store::api_cache_get/put` accessors (sqlite_orm has no JSONB column type, so the accessors use the store's own raw-connection path with `jsonb(?)` / `json(payload)`).
- Look-aside in **`call_cached`** for every stale-opt-in method (`search`, `detail`, `chapters`, `pages`, `genres`): fresh row within `ttl_s` is served directly (no plugin call); on plugin success the envelope is upserted as JSONB. Failure falls back to the last-good row with `cached:true` — the file pool is gone, not layered underneath.
- Key = sha1(plugin | method | canonicalized params) — key order does not affect hits.
- **The stale file cache pool `data/cache/api/` is removed**: cache root loses the `api` subpool, boot scan/legacy-purge/LRU accounting no longer reference it, and `call_cached`/`http_api.cpp` stop calling `Cache::store/peek(..., "api", ...)`.
- Constant per-method TTL (`kApiCacheTtl`, 24h) — ponytail: knob later.
- Out of scope: manual invalidation endpoint, per-user scope, full-text search, pruning policy (LRU/cap), other read models.

## Capabilities

### New Capabilities

- `search-cache`: sqlite JSONB read model for plugin API responses with fresh-serve / expiry-refresh / stale-serve via `call_cached` — replacing the api file pool.

### Modified Capabilities

(none)

## Impact

- Files: `src/store/store.{hpp,cpp}` (table + JSONB accessors, raw-connection path kept inside the store), `src/http/stale.hpp` (look-aside, stale-serve from DB, `Cache&` dependency dropped), `src/http/http_api.cpp` (call sites updated), `src/cache/cache.{hpp,cpp}` (api subpool removed), `tests/test_store.cpp`, `tests/test_cache.cpp`/`tests/test_stale*` as applicable.
- Risk: one cache layer instead of two — rollback = restore the file pool from git history. Migration of existing `.bin` files: dropped, not imported (they are sha1-keyed JSON; a one-shot import is not worth it — first boot re-fetches on miss).
