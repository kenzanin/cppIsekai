---
type: reference
title: image-cache deferred polish backlog
description: "Fix-later items dari final review image-cache (cache.cpp ec handling, main.lua guard, envelope_data test, ponytail accepts) + catatan MangaDex@Home emergency mode probe 2026-11-07."
tags: [backlog, polish, cache, review-findings, probe]
generated: { by: agent/mcp, at: "2026-11-07T10:05:00Z" }
status: stable
---

## Must-do at next polish pass (alem-than-or-equal 3 baris each)

1. **`src/cache/cache.cpp:52-56`** — error_code dari `file_size`/`last_write_time` in boot scan tidak dicek; race file hilang mid-scan can corrupt running `bytes_` until next rebuild. Fix: `if (fec) continue;` after both calls.
2. **`src/cache/cache.cpp:47-48`** — `create_directories` ec diabaikan; root unwritable = silent always-miss, needs a diagnostic (or ponytail note).
3. **`plugins_dev/lua/mangadex/main.lua:237-242`** — guard terror/broken belum cek `type(body.chapter.hash)` dan `type(entry data[])` (nil-hole truncation silent, concat nil error). ~2 baris.
4. **`tests/test_stale.cpp`** — belum ada test khusus untuk `envelope_data` helper (data array extraction).

## Accept-with-ponytail (no fix needed now)

- `stale.hpp:56` separator `|` ambiguity di key sha1 — theoretis, plugin id fixed dari manifest.
- `curl_global_init` absent — pre-existing.
- `plugin.hpp` curl include layering.
- worker loop exception guard — realistic fallback bad_alloc saja.
- worker nudge bisa `std::move`.
- test name "reuse after stop" overstated (constructs fresh).
- single-mutex fetch ceiling — terdocumentasi ponytail cache.cpp:98-100.

## Probe note (2026-11-07)

MangaDex@Home upstream sedang mode darurat: endpoint `/at-home/server/*` returns `"hash":""` + `"data":[]` for all chapters — **bukan bug plugin** (task 3.1 guard bekerja benar dengan `"at-home: malformed response"`). Live probe offline task 4.2 dapat diandalkan store murah memakai jalur email/update/pront mangadex `search/detail/chapters` stale + plugin fixture deterministic untuk path window/worker/image.

# Related Concepts
- [cppIsekai image-cache + offline-first pathway](../architecture/image-cache-offline.md): The decision this backlog polishes — every item here is inherited from that implementation.
- [cppIsekai core architecture: plugin-per-site manga viewer](../architecture/manga-viewer-core.md): Host architecture the cache/proxy/stale layers snap into.
