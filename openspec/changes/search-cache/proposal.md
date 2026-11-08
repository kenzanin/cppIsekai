# Proposal

## Why

Melihat ulang hasil pencarian harus memanggil plugin (HTTP ke site) setiap kali, padahal
respons deterministik terhadap query. Keputusan OKF `decisions/search-read-model-cache`
(user approve 2026-11-08) menyimpan hasil sebagai read-model JSONB di sqlite lewat
`call_cached`, selaras probe jsonb `normalization-contract`.

## What Changes

- Tabel `search_cache(plugin_id, query_hash PK, params_json, results JSONB, fetched_at, ttl_s)` di store (satu-satunya lapisan SQL).
- Lookaside di **`call_cached`**: method `search` opt-in — sebelum memanggil plugin, cek cache fresh; setelah sukses, write-through upsert. TTL konstanta per method (ponytail: knob ini belakangan).
- Kunci = sha1(query + opts kanonik) — urutan key tidak mempengaruhi hit.
- File-cache stale existing TETAP (fallback offline); DB = read-model terstruktur.
- Out of scope: method lain (pola menyusul), invalidation manual, UI.

## Capabilities

### New Capabilities

- `search-cache`: read-model hasil pencarian di sqlite (JSONB) dengan serve fresh / expiry refresh via `call_cached`.

### Modified Capabilities

(none)

## Impact

- Files: `src/store/store.{hpp,cpp}` (migrasi + accessor), `src/http/http.cpp` (`call_cached`), `src/plugin/plugin.cpp` (helper key — atau di store), `tests/test_store.cpp`/`test_plugin.cpp`.
- Risiko: write path ganda (DB + file cache) — diterima; pembersihan menyusul bila terbukti redundan.
