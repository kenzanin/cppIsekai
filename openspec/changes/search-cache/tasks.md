# Tasks

## 1. Store

- [ ] 1.1 Migration + `search_cache` model (columns per design D1) + `search_cache_get/put` accessors behind the Pimpl; round-trip unit test

## 2. Cache path

- [ ] 2.1 Canonical-key helper (sort keys, sha1-16) + unit tests (key order, types, per-plugin isolation)
- [ ] 2.2 `call_cached` look-aside+write-through for `search` (D3); constant ttl; identical envelope shape

## 3. Verify

- [ ] 3.1 Integration: second hit without a plugin call (counter fixture), expiry via ttl=0; full ctest green zero new warnings; `openspec validate --specs`
