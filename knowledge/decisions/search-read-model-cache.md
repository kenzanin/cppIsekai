---
type: decision
title: "Search results read-model: JSONB cache via call_cached"
description: "User approved 2026-11-08: plugin results as a JSONB read model via opt-in call_cached; TTL policy extended 2026-11-18 (d423b11): per-method host default + per-plugin manifest cache_ttl override."
tags: [cache, sqlite, jsonb, search, performance, read-model]
generated: { by: agent/mcp, at: "2026-11-18T13:58:35Z" }
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

## TTL policy v2 (2026-11-18, d423b11): per-method default + manifest override

Reader page URLs can carry **short-lived signed session tokens** (comico `d=` from
/reader/access decays in minutes -> every /cache/img fetch 410s on a stale row,
blank reader). Uniform 24h TTL served dead tokens, so:

- Host per-method default `cache_ttl_for(method)` (src/http/stale.hpp): `pages` = 30 min,
  everything else 24h. Cheap insurance for every token-minting site.
- **Per-plugin override in main.json**: `"cache_ttl": {"pages": 1800}` (seconds,
  per-method, optional). Parsed into PluginInfo (`plugin_load.cpp`), exposed via
  `Host::cache_ttl_seconds(id, method)` (0 = absent), applied in `call_cached` via
  `effective_ttl`: manifest override -> host per-method default -> 24h.
- Site knowledge (token lifetime) lives in the plugin manifest; the host stays
  generic. All five call_cached sites pass `&plugins`.
- Local-first contract unchanged: an expired row still serves (flagged
  `cached:true`) — TTL only bounds the fresh window and flags the UI badge; it does
  NOT force a re-mint (decisions/local-first-reads).
- test_stale/test_pages xmake reqs gained `thread-pool` (they now include plugin.hpp).

## Related

- [Normalisation contract: MangaDex canonical + chapter release_at](../architecture/normalization-contract.md): Source of the jsonb/hybrid decision.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): jsonb DB-side only, never the wire.
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): Store is the only SQL; cache via the store layer.
