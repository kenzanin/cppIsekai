# Tasks

## 1. Store

- [ ] 1.1 Migrasi + model `search_cache` (kolom sesuai design D1) + accessor `search_cache_get/put` di belakang Pimpl; unit round-trip

## 2. Cache path

- [ ] 2.1 Helper canonical-key (sort keys, sha1-16) + unit test (urutan key, tipe, plugin isolasi)
- [ ] 2.2 `call_cached` lookaside+write-through untuk `search` (D3); ttl konstanta; shape envelope identik

## 3. Verify

- [ ] 3.1 Integrasi: hit-2 tanpa plugin call (counter fixture), expiry via ttl=0; full ctest green zero new warnings; `openspec validate --specs`
