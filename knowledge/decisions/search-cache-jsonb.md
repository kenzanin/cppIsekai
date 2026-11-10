---
type: decision
title: "search-cache: api read model as JSONB in sqlite"
description: "search-cache (2026-11-10, branch search-cache, commit cd5d515): api read model dipindah ke sqlite JSONB binary — tabel api_cache(plugin_id, query_hash PK, params_json, payload BLOB, fetched_at, ttl_s), payload via jsonb(?)/json(payload) lewat raw sqlite3_* DI DALAM src/store/ saja (sqlite_orm tidak punya JSONB mapping). call_cached kini look-aside fresh (tanpa plugin call, tanpa cached flag), write-through upsert kApiCacheTtl=24h, failure -> last-good + cached:true; kApiCacheTtl ponytail knob. File pool data/cache/api DIHAPUS (boot scan/LRU/purge img-only); .bin dropped tidak dimigrasi. /api/refresh memakai stale_key+api_cache_put. Live probe :8090 /api/detail 2x ok, row JSONB masuk. ctest 14/14, specs 15/15. Menggantikan decisions/search-read-model-cache."
generated: { by: agent/mcp, at: "2026-11-10T09:19:53Z" }
status: stable
---


