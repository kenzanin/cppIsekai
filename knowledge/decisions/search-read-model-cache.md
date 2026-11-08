---
type: decision
title: "Search results read-model: JSONB cache via call_cached"
description: "User approve 2026-11-08: hasil search plugin jadi read-model JSONB di sqlite (search_cache: query_hash, params, results, fetched_at, ttl_s) lewat call_cached opt-in; fresh = SELECT tanpa HTTP; file-cache stale tetap; implementasi via change search-cache."
tags: [cache, sqlite, jsonb, search, performance, read-model]
generated: { by: agent/mcp, at: "2026-11-08T09:00:00Z" }
status: stable
---

Jawaban atas "cache operasi berat di sqlite jsonb?" - **ya, untuk search dulu**, selaras probe `normalization-contract` (jsonb_* family lolos di libsql 3.47) dan prinsip jsonb = DB-side only.

## Desing inti

- Tabel `search_cache(plugin_id, query_hash PK, params_json, results JSONB, fetched_at, ttl_s)`; `query_hash` = sha1(query + opts kanonik).
 Aliran: cek DB dulu (fresh menurut ttl) -> `SELECT`, tanpa menyentuh plugin; miss/expired -> panggil plugin -> write-through upsert.
- Integrasi di **`call_cached`** (satu wrap point existing), opt-in per method - `search` terlebih dahulu; operasi berat lain menyusul dengan pola materialized read-model yang sama, BUKAN cache-all.
- Hubungan dengan file-cache stale existing: file cache tetap (fallback offline umum untuk raw response); DB = read-model terstruktur untuk search (query/filter server-side, union history). Ponytail: bila terbukti dobel simpan, pangkas salah satu.
- TTL: konstanta per method dulu (ponytail: jadi knob ini belakangan; hindari sprawl konfigurasi di change pertama).

## Related

- [Normalisation contract: MangaDex canonical + chapter release_at](../architecture/normalization-contract.md): Sumber keputusan jsonb/hybrid yang dipakai.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): jsonb DB-side only, never the wire.
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): Store satu-satunya SQL; cache lewat layer store.
