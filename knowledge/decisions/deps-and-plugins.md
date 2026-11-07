---
type: decision
title: "Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto"
description: "User-confirmed 2026-11-07: curl-first REST routes (CBZ export = host route), plugin runtime parity same signature (quickjs-ng second runtime), no hand-rolled crypto (stbrumme/hash-library md5/sha1), base64 dari glaze, libzippp + pacman libzip, quickjs-ng vendored static."
tags: [dependencies, cbz, quickjs, plugins, hashing, api-design]
generated: { by: agent/mcp, at: "2026-11-07T10:20:00Z" }
status: stable
---

Keputusan-keputusan ini di-stack di atas decisions/third-party-dependencies dan architecture/plugin-system.

## R1 - Curl-first APIs
SEMUA feature aplikasi harus bisa diakses via curl (REST routes) agar mudah debug dan test. Implikasi langsung: CBZ export = host route `GET /export/cbz?manga=<id>&chapters=...`, BUKAN plugin capability.

## R2 - Plugin runtime parity
Semua plugin runtime (lua, js) punya feature set & signature yang SAMA — quickjs-ng sebagai runtime binding kedua dalam data-contract ABI yang sudah ada (architecture/plugin-system). JANGAN buat contract branch baru; host-tools binding per-runtime mengikuti ABI existing. wasm3 tidak diprioritaskan lagi (tetap vendored, tidak dijadwalkan).

## R3 - No hand-rolled crypto
Hapus `src/cache/sha1.hpp` (implementasi sendiri); semua hashing pakai stbrumme/hash-library (vendored). md5 menggantikan sha1 untuk cache/stale keys; sha1 juga dari library bila ada yang butuh. TIDAK menambah cpp-base64 — base64 pakai yang sudah ada (glaze).

## R4 - CBZ via libzippp + libzip system
pacman `libzip` (system-shared first per decisions/third-party-dependencies), libzippp header vendored (wrapper ~2 source). Export ke CBZ dari host route, bukan plugin.

## R5 - quickjs-ng vendored static
Apache-2.0, tanpa dependency eksternal. Menggantikan rencana "wasm3 second" dengan "js second".

## Impact scope

Change openspec baru `deps-and-plugins`, di-stack di branch image-cache (file yang sama: cache keys, plugin host, routes).
