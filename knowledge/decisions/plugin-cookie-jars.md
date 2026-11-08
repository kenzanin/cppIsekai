---
type: decision
title: "Plugin cookie jars: libcurl-native, per-plugin file state"
description: "User approve 2026-11-08: cookie engine native libcurl (CURLOPT_COOKIEFILE/JAR), satu jar per plugin di data/plugin-state/<id>.cookies.txt, diaktifkan lewat opsi fetch gate; tanpa dependensi baru; implementasi menyusul di change fetch-gate."
tags: [cookie, curl, plugin-state, fetch, security]
generated: { by: agent/mcp, at: "2026-11-08T09:00:00Z" }
status: stable
---

Jawaban atas "cookie jar perlu library?" - **tidak**; libcurl sudah membawa engine-nya.

## Kebijakan

- **Engine**: `CURLOPT_COOKIEFILE` (path jar; string kosong = enable tanpa load) + `CURLOPT_COOKIEJAR` (persist format Netscape saat cleanup). Inspeksi via `CURLOPT_COOKIELIST`/`CURLINFO_COOKIE_LIST`.
- **Lokasi**: `data/plugin-state/<id>.cookies.txt` - sengaja di data/, BUKAN folder plugin (folder plugin read-only per `decisions/plugin-data-segregation`; state memang dipisah).
- **Isolasi**: handle curl dibuat fresh per request + load jar milik sendiri => antar-plugin terisolasi gratis; persist antar-restart otomatis.
- **Kontrol**: opsi `cookie_jar` (path) di `fetch::Request` - gate `src/fetch/` yang memasang; kosong = cookie engine off (perilaku sekarang).

## Relevansi

Mangadex memakai `storage.*` untuk token OAuth; jar ini untuk site yang bergantung session cookie (CF clearance, dsb). Implementasi menyusul di change `fetch-gate` (titik tembus sama dengan gate impersonate).

## Related

- [Plugin data segregation: host .ini is host-only](plugin-data-segregation.md): Alasan jar di data/, bukan folder plugin.
- [Fetch profile gate: centralized curl-impersonate, dynamic per-call profile](fetch-profile-gate.md): Gate yang memasang cookie engine.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): Opts plugin yang ditambah field cookie.
