# Proposal

## Why

`curl_easy_impersonate` dipanggil dari dua tempat (plugin.cpp `fetch_sync` :261 dan jalur lua legacy :562) dan profile efektif dipatok saat plugin load — mengganti fingerprint TLS butuh restart. Cookie juga tidak persist di mana pun. User steer 2026-11-08 (OKF `decisions/fetch-profile-gate`, `decisions/plugin-cookie-jars`): satu gate terpusat, profile dinamis per-call, dan jar persist per-plugin.

## What Changes

- **Modul `src/fetch/` (baru)**: `fetch::Request{url, timeout_ms, profile?, cookie_jar?, method, body, headers...}` + `fetch::sync(Request, proxy)` — satu-satunya call site `curl_easy_impersonate` di tree; sink/retry-ponytail/`applyProxy` pindah ke sini; duplikat :561-area dilipat.
- **Profile dinamis**: opts plugin (lua/js/wasm) dapat `profile` per-call; resolusi `opts > plugin.fetch_profile (ini)`; **default live** — conf dibaca saat call via snapshot, jadi `POST /config` efektif tanpa reload; target invalid => warn + fallback default.
- **Cookie jar**: opsi `cookie_jar` (path) di gate; helper jar per-plugin `data/plugin-state/<id>.cookies.txt`; `CURLOPT_COOKIEFILE/JAR` native; off by default (perilaku lama saat absen).
- Out of scope: firefox target (butuh .so `-firefox` terpisah — phase 2), cache read-model search (change `search-cache`), UI cookie.

## Capabilities

### New Capabilities

(none — perilaku fetch host; requirement masuk delta `plugin-system`)

### Modified Capabilities

- `plugin-system`: menambah requirement **Per-call impersonation profile** dan **Persistent per-plugin cookie jar** (ADDED saja; requirement existing tidak berubah).

## Impact

- Files: `src/fetch/fetch.{hpp,cpp}` (baru), `src/plugin/plugin.{hpp,cpp}` (consumer + hapus duplikat), `tests/test_plugin.cpp`, `CMakeLists.txt` (target source+link), fixture smoke.
- Tanpa dependensi baru; lib & target mengikuti probe OKF (chrome99-116/edge99-101/safari15).
- Risiko: perilaku http existing harus identik (parity) — dijaga test existing + smoke; penyentuhan jalur panas semua plugin.
