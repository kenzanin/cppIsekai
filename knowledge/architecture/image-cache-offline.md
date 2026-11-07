---
type: decision
title: cppIsekai image-cache + offline-first pathway
description: "Decision: host-side disk-LRU cache (data/cache/{img,api}, sha1 keys, cache_max_mb ini 2048MB, mem-index+mtime rebuild) + proxytigen host-wide bogus proxy offline sim; /cache/img serve-through; stale-serve last-good +cached:true di satu wrap point ABI (optin search/detail/chapters/pages/genres); mangadex pages via /at-home/server/{id} data[]; reader position/skip snprintf ke host/skip/<id> state; sliding 10-page window pure fn; worker header-only nudge-only sequential delta fetch; live probe verified offline. Task 1.1-4.2, Subagent-driven with per-task reviews."
tags: [rewrite, cache, offline, proxy, stale, reader, mangadex]
generated: { by: agent/mcp, at: "2026-11-07T09:47:03Z" }
status: stable
---

## The problem

cppIsekai harus local-first: dengan network unreachable (simulasi `proxy = http://127.0.0.1:1` di `data/cppisekai.ini`), manga yang pernah dibuka tetap bisa dipakai — metadata, daftar chapter, dan gambar halaman — dari cache. Sebelumnya setiap view menendang plugin live, gambar hot-link ke CDN, dan tidak ada yang bertahan offline.

## Design yang diambil

**1. One cache, one LRU pool.** `data/cache/img/<sha>.<ext>` dan `data/cache/api/<sha>.<ext>` baris satu LRU index dalam memori (path, bytes, tick) — rebuild saat boot dari scan disk (mtime = recency), recency dipantau saat serve, evict whole-file on insert overflow. Knob tunggal: `[cache] cache_max_mb` (default 2048).

**2. Proxy is host-wide and dumb.** `proxy=` inivalue => CURLOPT_PROXY on every outbound handle (plugin http.fetch + image getter) via a single plugin::applyProxy guard; absent/empty = direct, no mode flag.

**3. Stale-serve at the ABI route, keyed sha1(plugin|method|sorted-query).** One wrap (`call_cached`) around the one plugin-call site; success persist envelope + recency; failure (`ok:false`/malformed) => peek last-good + injecting `"cached": true` top-level via glaze. Opt-in methods: search, detail, chapters, pages, genres.

**4. Chapter order and page lists from plugin, cached like everything else.** mangadex `pages` via `/at-home/server/{chapterId}` => `https://{baseUrl}/data/{hash}/{file}` per `chapter.data[]` (dataSaver OFF). No DB chapter table.

**5. Window math is a pure function.** `window(chapters, pages_of, position, skipped) -> [urls]`: flatten ascending, exclude skips, start after current page, take 10. Header-only injectable; worker (header-only `reader::Worker`) dedups concurrent misses via cache ensure hit-path; nudge-only (no timer), driven by POST /reader/position and POST /chapter/<id>/skip; sequential delta fetch; drain+join stop.

**6. Skip flags live in `plugin_state` under reserved `host/skip/<chapter_id>` keys** (upsert existing, zero schema change).

**7. Serve-through image route.** `GET /cache/img?url=` — content-type dari ext, disk hit atau fetch+store.

## Verification snapshot (2026-11-07)

- `ctest` 7 target hijau (hide, plugin, oracle, http_img, stale, jendela, worker); openspec `image-cache` valid.
- Live probe bogus-proxy: mangadex search/detail/chapters offline = keys `{ok, data, cached}`; image dari disk `200 image/png`; unviewed image 404; unviewed search `{ok:false, error}` tanpa flag; window boundary-pass [c1p2, c2p1, c2p2, c3p1, c3p2] dengan skip-c2 reroute [c1p2, c3p1, c3p2].
- Final whole-branch review: Merge-Ready yes, 6/6 requirement `spec` end-to-end, 0 big integration risk, deferred polish backlog di `references/image-cache-backlog.md`.

# Related Concepts
- [image-cache deferred polish backlog](../references/image-cache-backlog.md): polishing items produced by this change's final review.
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): host architecture the cache/proxy/stale layers snap into.
