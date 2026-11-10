# Tasks

## 1. Store JSONB read model
- [x] 1.1 Migration + `api_cache` table (columns per design D1: `payload BLOB` via `jsonb(?)`, `json(payload)` on read) + `api_cache_get/put` accessors behind the Pimpl, raw `sqlite3_*` confined to `src/store/`; round-trip unit test incl. JSONB byte semantics

## 2. Cache path
- [x] 2.1 `call_cached` look-aside + write-through for all stale-opt-in methods (`search|detail|chapters|pages|genres`) per design D3; last-good served with top-level `cached:true`; store failure envelope verbatim when no row; drop `Cache&` dependency (or keep unused? no — remove)
- [x] 2.2 Remove the api file pool: `data/cache/api/` deleted, `Cache` loses the `api` subpool (boot scan, legacy purge, LRU accounting, `store/peek` api call sites in `stale.hpp` and `http_api.cpp` `/api/refresh` rewired to store accessors)

## 3. Verify
- [x] 3.1 Tests: canonical-key unit tests (order, drop empties, per-plugin isolation), store round-trip, call_cached second-hit-no-plugin-call (counter fixture), expiry via ttl_s=0, stale-serve cached flag; full ctest green, zero new warnings; `openspec validate --specs`
