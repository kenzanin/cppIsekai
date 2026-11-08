---
type: decision
title: "Search results read-model: JSONB cache via call_cached"
description: "User approved 2026-11-08: plugin search results become a JSONB read model in sqlite (search_cache: query_hash, params, results, fetched_at, ttl_s) via opt-in call_cached; fresh = SELECT, no HTTP; the file cache stays; implementation via change search-cache."
tags: [cache, sqlite, jsonb, search, performance, read-model]
generated: { by: agent/mcp, at: "2026-11-08T09:00:00Z" }
status: stable
---

Answer to "cache heavy host ops in sqlite jsonb?" - **yes, search first**, aligned with
the `normalization-contract` probe (jsonb_* family verified on libsql 3.47) and the
jsonb = DB-side only principle.

## Core design

- Table `search_cache(plugin_id, query_hash PK, params_json, results JSONB, fetched_at, ttl_s)`;
  `query_hash` = sha1(query + canonicalized opts).
  Flow: check DB first (fresh per ttl) -> `SELECT`, no plugin touch; miss/expired ->
  plugin call -> write-through upsert.
- Integration in **`call_cached`** (the existing single wrap point), opt-in per method -
  `search` first; heavier ops follow the same materialized read-model pattern, NOT
  cache-all.
- Relationship to the existing file cache: the file cache stays (general offline
  fallback for raw responses); the DB is the structured search read model (server-side
  filter/sort, history union). Ponytail: prune one side if duplication proves real.
- TTL: constant per method first (ponytail: becomes an ini knob later; avoid config
  sprawl in the first change).

## Related

- [Normalisation contract: MangaDex canonical + chapter release_at](../architecture/normalization-contract.md): Source of the jsonb/hybrid decision.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): jsonb DB-side only, never the wire.
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): Store is the only SQL; cache via the store layer.
