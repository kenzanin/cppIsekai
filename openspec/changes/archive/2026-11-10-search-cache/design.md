# Design

## Context

`call_cached` (src/http/stale.hpp) is the single ABI wrap point; the store (sqlite_orm behind Pimpl, one connection, `src/store/`) is the only SQL layer. `data/cache/api/*.bin` is a sha1-keyed JSON pile used as the stale-serve fallback for `search|detail|chapters|pages|genres` — no TTL, no structure. OKF `decisions/search-read-model-cache` (2026-11-08) and the 2026-11-10 direction lock this change: api read model moves into sqlite as **JSONB binary**, gated behind the store.

## Goals / Non-Goals

**Goals:** repeat call = SELECT; identical envelope shape; all stale-opt-in methods covered; offline-safe via last-good JSONB row (`cached:true`); **the api file pool is removed** — one cache layer.

**Non-Goals:** pruning/LRU/cap policy; manual invalidation endpoint; per-user scope; full-text search; migrating existing `.bin` files (dropped, re-fetched on miss).

## Decisions

- **D1 — Schema.** `api_cache(plugin_id TEXT, query_hash TEXT, params_json TEXT, payload BLOB, fetched_at INTEGER, ttl_s INTEGER, PRIMARY KEY(plugin_id, query_hash))`. `payload` is written with `jsonb(?)` and read with `json(payload)`. sqlite has no native JSONB column type; sqlite_orm has no JSONB mapping — the accessors go through the store's own raw-connection path (`sqlite3_*` inside `src/store/` only). Column policy per `architecture/normalization-contract` (slim-JSON; the envelope's `data` is stored verbatim, not re-interpreted).
- **D2 — Key.** `canonical(params)`: recursive key sort, drop empties, sha1 hex (first 16); plugin_id and method are folded into the hash input (`plugin|method|canon`) so per-plugin isolation holds. Reuses the existing `stale_key` shape (glaze `generic_sorted`), no new canonicalizer. Pure helper + unit tests.
- **D3 — Flow in `call_cached`.**
  1. Compute key; `Store::api_cache_get(plugin, hash)` → if row exists and `fetched_at + ttl_s > now` → return envelope with entry as stored (no plugin call).
  2. Else invoke plugin; if `ok` → `Store::api_cache_put(...)` upsert with `fetched_at=now`, `ttl_s=kApiCacheTtl`; return envelope.
  3. Else (`ok:false` / malformed / plugin threw) → look up last-good row; if present → return it with top-level `cached:true` merged via glaze; else return the failure envelope unchanged.
  Fresh-serve needs **no** `cached` flag. Stale-serve sets it. ponytail: constant 24h `kApiCacheTtl`; per-method knob later.
- **D4 — Removal of the file pool.** `Cache` loses the `api` subpool: boot scan skips it, `legacy_v1`/purge/LRU accounting reference `img` only, `store/peek(..., "api", ...)` call sites (`stale.hpp`, `http_api.cpp` `/api/refresh`) are rewired to the store JSONB accessors. `data/cache/api/` is deleted from disk and the repo's cache root contract becomes img-only. The img pool is untouched.
- **D5 — Store access.** `Store::api_cache_get(plugin, query_hash) -> std::optional<ApiCacheRow>` and `Store::api_cache_put(...)` behind the Pimpl. Raw `sqlite3*` connection is opened **inside** the store (one-shot handles per call are fine at this rate, or a cached handle in `StoreImpl` — implementer's call, same pattern as the existing `reader_positions` ALTER migration). No SQL text leaves `src/store/`.
- **D6 — Staleness semantics.** `fetched_at` is unix seconds. Expiry = `now >= fetched_at + ttl_s` → refresh. `ttl_s <= 0` means always-expired (test hook, mirrors search-cache's ttl=0). Failure with no row → return the plugin's failure envelope verbatim (same as today's no-stale-file case).
- **D7 — Testing.** Key canonicalization unit tests (order, drop empties, per-plugin isolation); store round-trip incl. `jsonb()` write and `json()` read byte-equality; `call_cached` fixture plugin (second hit without plugin call, asserted via call counter); expiry via `ttl_s=0`; stale-serve flag merge; full `ctest` green with zero new warnings; `openspec validate --specs`.

## Risks / Trade-offs

- Raw-SQL path inside the store is a deliberate exception to "only sqlite_orm" — forced by JSONB typing; it stays behind the Pimpl and is the only place `sqlite3_exec`/`sqlite3_bind_blob` for api_cache may appear.
- Dropping `.bin` files means one cold boot (re-fetch) — accepted, offline daemons will serve stale only after the first successful call post-upgrade.
- Single layer = no independent file fallback if the DB is corrupt; mitigation = `cached:true` last-good row, plus standard sqlite WAL recovery.
