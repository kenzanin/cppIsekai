---
type: Fact
title: "keroscans plugin: endpoints + decimal-subchapter gotcha"
description: "keroscans.net plugin: custom theme scrape map (search/detail/chapters partial pager/pages) + decimal subchapter gotcha."
tags: [plugins, lua, keroscans, scrape]
generated: { by: agent/mcp, at: "2026-11-25T16:20:38Z" }
status: stable
---

## Site
keroscans.net — custom theme (bukan Madara). Plugin `plugins_dev/lua/keroscans/` (api 1, Lua): search/detail/chapters/pages.

## Endpoints (dari HAR tmp/keroscans.har, live-verified 2026-11-25)
- search: `GET /search?q=` → `<article data-series-id>` per card; slug `/series/comic/<slug>`, cover `src`, judul `<h3>`. Tanpa pagination marker.
- detail: og:* meta + facts `<dt>Author/Artist/Status</dt><dd>`; genres dari JSON-LD `"genre":[...]`.
- chapters: page 1 embedded di series page; older = `GET /series/comic/<slug>?page=N&partial=chapters` (wrapper #chapter-list utuh, 10 row/halaman) loop sampai 0 row baru. Row: `data-chapter-number` + `<time datetime=ISO>`.
- pages: `GET /series/comic/<slug>/chapter/<n>`; `<li class="ks-page"><img src="/media/pages/..."` urut DOM; cover pakai `/media/covers/` (prefix path = pembeda). Chapter id composite `slug:number`.

## Gotchas
- Nomor chapter DESIMAL (25.2) — `data-chapter-number` dan id composite harus `[0-9.]+`, jangan `\d+` (23 dari 56 chapter kena buang saat implementasi pertama).
- `call_cached`: edit scraper = `POST /plugins/cache-clear` sebelum retest, else hasil lama.
- cloudflare di depan; UA Chrome + `image_referer` di manifest. rate-limit header x-ratelimit-limit 60.

generated: { by: "kit-agent", at: "2026-11-25" }

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md): keroscans plugin instance of the folder/manifest/ABI contract
