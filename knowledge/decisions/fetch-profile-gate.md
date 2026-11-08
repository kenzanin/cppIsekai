---
type: decision
title: "Fetch profile gate: centralized curl-impersonate, dynamic per-call profile"
description: "User steer 2026-11-08: gate impersonate pindah ke src/fetch/ (satu-satunya call site), profile per-call dari opts plugin > fetch_profile ini, default live via POST /config; targets lib ter-link chrome99-116/edge99-101/safari15; firefox = phase 2 (.so terpisah); menolak profile-class JA3 hand-rolled."
tags: [fetch, curl-impersonate, plugin-api, config, performance]
generated: { by: agent/mcp, at: "2026-11-08T08:30:00Z" }
status: stable
---

Keputusan ini mengeksekusi arah user dari diskusi `tmp/curl-impresionate.md` (demo BrowserProfile): dynamic profile switching TANPA class JA3 hand-rolled.

## D1 - Gate terpusat di src/fetch/

`src/fetch/fetch.hpp/.cpp` = satu-satunya tempat `curl_easy_impersonate` di seluruh tree. `fetch::Request{url, timeout_ms, profile?, method, body, headers...}` + `fetch::sync(Request, proxy) -> {status, body, headers, error}`. `fetch_sync` + `applyProxy` + sink pindah dari plugin.cpp; duplikat call-site impersonate (:261 retry & :562 legacy lua) ikut hilang. Retry ponytail existing dipertahani di gate.

## D2 - Resolusi profile & default live

Precedence: `opts.profile` (per-call, dari plugin lua/js/wasm) > `plugin.fetch_profile` ini. Default LIVE: konf dibaca saat call (shared snapshot), bukan di-capture closure saat load -> `POST /config plugin.fetch_profile=...` efektif untuk call berikutnya tanpa restart/reload.

## D3 - Targets & phase

Lib ter-link (`libcurl-impersonate-chrome.so.4.8.0`) menerima: chrome99/100/101/104/107/110/116, edge99/101, safari15 (hasil probe strings). Phase 1 = target set ini saja. Phase 2 = firefox: butuh vendor `libcurl-impersonate-firefox` terpisah (TLS NSS) - follow-up, tidak mengubah gate.

## D4 - Ditolak: profile-class manual

UA/headers/cipher/EC-curves per-browser (gaya demo) tidak diimplementasi - `curl_easy_impersonate` sudah bake semuanya per target; klas hanya menambah drift terhadap fingerprint lib.

## Follow-up (di luar change ini)

Cookie jar persist per-plugin dan cache read-model search (JSONB) - lihat diskusi 2026-11-08; masing-masing jadi keputusan/change sendiri.

## Related

- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](deps-and-plugins.md): Ruling curl-first yang gate ini turun-terapan.
- [cli-config: registry-backed config surface](../architecture/cli-config.md): Pemilik knob `plugin.fetch_profile` yang jadi fallback default.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md: ABI yang opts-nya ditambah field `profile`.
