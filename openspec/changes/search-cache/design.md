# Design

## Context

`call_cached` (src/http/http.cpp) adalah satu-satunya wrap point ABI; store sqlite_orm
di belakang Pimpl (`src/store/`), satu koneksi; jsonb policy terkunci di
`architecture/normalization-contract` (variant metadata slim JSON / filtered columns).
Motivasi & scope: proposal.md.

## Goals / Non-Goals

**Goals:** repeat-search = SELECT; transparan bagi plugin & route (shape identik);
opt-in per method; aman offline (fall-back ke plugin/file-cache path existing).

**Non-Goals:** cache semua method; invalidation endpoint; per-user scope; full-text
(index menyusul bila perlu).

## Decisions

- **D1 — Skema.** `search_cache(plugin_id TEXT, query_hash TEXT, params_json TEXT,
  results JSONB, fetched_at INTEGER, ttl INTEGER, PK(plugin_id, query_hash))`;
  results disimpan sebagai JSON string (sqlite tidak punya tipe native — akses via
  `json_*`/`jsonb_*` libsql; policy kolom mengikuti normalization-contract).
- **D2 — Kunci. ` `canonical(params)`: sort keys rekursif, drop empty, sha1 hex 16 char
  pertama; plugin_id digabung di PK (isolasi antar-plugin). Helper murni + unit test.
- **D3 — Aliran di `call_cached`.** Param method `search`: lookaside dulu (fresh =
  return envelope `{"ok":true,"data":<results>}` dari kolom); else plugin call; ok =>
  upsert (results = data JSON), fail => perilaku existing (stale layer menangani).
  TTL konstanta 24h `kSearchCacheTtl` (ponytail: knob ini).
- **D4 — Koeksistensi.** File-cache stale layer tidak berubah; DB read-model hanya
  menambah lapisan read. Ponytail tercatat: dedupe bila redundan.
- **D5 — Store access. ` `Store::search_cache_get(plugin,hash)` /
  `search_cache_put(...)` di belakang Pimpl; migrasi tambah tabel (sqlite_orm sync).
- **D6 — Testing. Unit canonicalizer; integrasi store round-trip; perilaku call_cached
  dengan fixture plugin (hit ke-2 tanpa plugin call — assert via call counter fixture);
  ttl=0 untuk expirasi test. Network-free.

## Risks / Trade-offs

- Envelope hasil harus re-konstruksi identik — simpan `data` utuh, bukan reinterpret.
- Ukuran tabel tumbuh — ponytail: cap/LRU per plugin menyusul.
