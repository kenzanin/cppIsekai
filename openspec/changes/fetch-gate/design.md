# Design

## Context

State saat ini & constraint: dua call-site impersonate (`fetch_sync` plugin.cpp dan blok
curl legacy jalur lua), profile dari `conf_.fetch_profile` di-capture closure saat load
plugin; sink/retry/`applyProxy` inline. Keputusan terkunci di OKF
`decisions/fetch-profile-gate` + `decisions/plugin-cookie-jars`. Motivasi: proposal.md.

## Goals / Non-Goals

**Goals:** satu gate (`src/fetch/`) — single impersonate call site; profile per-call +
default live; cookie jar persist per-plugin; parity perilaku http existing.

**Non-Goals:** firefox target (phase 2, .so terpisah); class JA3/UA manual; cache
read-model (change `search-cache`); per-call user-agent override manual.

## Decisions

- **D1 — Ownership pindah utuh.** `FetchResult`, sink body/header, retry impersonate,
  `applyProxy`, dan opsi-set block pindah ke `fetch::sync`; plugin.cpp hanya menyusun
  `Request`. Tidak ada logika curl yang tersisa di plugin.cpp (grep-able: satu
  `curl_easy_impersonate` di tree).
- **D2 — Resolusi profile & default live.` `Request.profile` kosong => default; snapshot
  config dibaca saat call (`std::shared_ptr<const Config>` di-host, diganti atomik oleh
  `POST /config`) — closure tidak lagi men-capture nilai konfigurasi. Target tak dikenal
  => warn + pakai default (request tetap jalan).
- **D3 — Cookie engine. ` `Request.cookie_jar` (path): `CURLOPT_COOKIEFILE(path)` +
  `CURLOPT_COOKIEJAR(path)`; kosong = tidak diset (perilaku lama byte-identical). Helper
  `plugin_state_dir/<id>.cookies.txt` di sisi plugin; kegagalan file => warn, request jalan
  tanpa persist.
- **D4 — ABI opts. ` `profile` (string, optional) & `cookie` (bool/obj minimal: enable)
  di lua `http.fetch` opts, js opts object, import wasm (`env.http_fetch` opts JSON) —
  field absen = today. Tidak mengubah envelope/response.
- **D5 — Parity legacy.** Jalur lua lama (:562) dilipat ke gate; response shape & retry
  ponytail dipertahani; diff behavior dijaga test existing (test_plugin 10/10 wajib).
- **D6 — Testing. Unit: validasi nama target + fallback, jar round-trip di tmp dir,
  isolasi dua jar; parity: test existing. Live smoke: env `PLUGIN_SMOKE_URL` style,
  opsional; ctest tetap network-free.

## Risks / Trade-offs

- Menyentuh jalur panas semua plugin — mitigasi parity test + pindahan mekanis.
- `POST /config` live-default: snapshot swap harus thread-safe (shared_ptr atomic) —
  pola sudah ada di store conf read.

## Open Questions

None; daftar target mengikuti probe OKF (chrome99-116/edge99-101/safari15).
