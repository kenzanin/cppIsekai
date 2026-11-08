# Design

## Context

`call_cached` (src/http/http.cpp) is the single ABI wrap point; the store is sqlite_orm
behind a Pimpl (`src/store/`), one connection; jsonb policy locked in
`architecture/normalization-contract` (slim-JSON variant metadata / filtered columns).
Motivation & scope: proposal.md.

## Goals / Non-Goals

**Goals:** repeat-search = a SELECT; transparent to plugins & routes (identical shape);
opt-in per method; offline-safe (falls back to the existing plugin/file-cache path).

**Non-Goals:** caching every method; an invalidation endpoint; per-user scope; full-text
(index later if needed).

## Decisions

- **D1 — Schema.** `search_cache(plugin_id TEXT, query_hash TEXT, params_json TEXT,
  results JSONB, fetched_at INTEGER, ttl INTEGER, PK(plugin_id, query_hash))`;
  results stored as a JSON string (sqlite has no native type — access via libsql
  `json_*`/`jsonb_*`; column policy per normalization-contract).
- **D2 — Key.** `canonical(params)`: recursive key sort, drop empties, first 16 hex of
  sha1; plugin_id folded into the PK (per-plugin isolation). Pure helper + unit tests.
- **D3 — Flow in `call_cached`.** For method `search`: look-aside first (fresh => return
  the `{"ok":true,"data":<results>}` envelope from the column); else plugin call; ok =>
  upsert (results = the data JSON), failure => existing behavior (the stale layer
  handles it). Constant 24h `kSearchCacheTtl` (ponytail: knob).
- **D4 — Coexistence.** The stale file-cache layer is unchanged; the DB read model only
  adds a read layer. Ponytail noted: dedupe if proven redundant.
- **D5 — Store access.** `Store::search_cache_get(plugin,hash)` /
  `search_cache_put(...)` behind the Pimpl; migration adds the table (sqlite_orm sync).
- **D6 — Testing. Canonicalizer unit; store round-trip integration; call_cached behavior
  with a fixture plugin (second hit without a plugin call — assert via a call-counter
  fixture); expiry via ttl=0. Network-free.

## Risks / Trade-offs

- The result envelope must be reconstructed identically — store `data` verbatim, no
  reinterpretation.
- Table growth — ponytail: per-plugin cap/LRU later.
