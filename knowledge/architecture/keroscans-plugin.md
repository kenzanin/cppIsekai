---
type: Fact
title: "keroscans plugin: endpoints + decimal-subchapter gotcha"
description: "keroscans.net plugin: custom theme scrape map (search/browse/detail/chapters partial pager/pages/genres) + decimal subchapter & Lua pattern gotchas."
tags: [plugins, lua, keroscans, scrape]
generated: { by: agent/mcp, at: "2026-11-25T16:48:55Z" }
status: stable
---

## Site
keroscans.net — custom theme (bukan Madara). Plugin `plugins_dev/lua/keroscans/` (api 1, Lua): search/detail/chapters/pages/genres.

## Endpoints (dari HAR tmp/keroscans.har, live-verified 2026-11-25)
- search: `GET /search?q=&page=N` → `<article data-series-id>` per card; slug `/series/comic/<slug>`, cover `src`, judul `<h3>`. Total di teks `N results`.
- browse (chip genre, q kosong): `GET /series?genre[i]=<slug>&page=N` — `/search` dengan q kosong mengembalikan 0 card. Total di teks `N series`.
- genres: `GET /genres` → chip `<a href="...genre%5B0%5D=<slug>">…<span>Name</span>` (19 genre).
- detail: og:* meta + facts `<dt>Author/Artist/Status</dt><dd>`; genres dari JSON-LD `"genre":[...]`.
- chapters: page 1 embedded di series page; older = `GET /series/comic/<slug>?page=N&partial=chapters` (wrapper #chapter-list utuh, 10 row/halaman) loop sampai 0 row baru. Row: `data-chapter-number` + `<time datetime=ISO>`.
- pages: `GET /series/comic/<slug>/chapter/<n>`; `<li class="ks-page"><img src="/media/pages/..."` urut DOM; cover pakai `/media/covers/` (prefix path = pembeda). Chapter id composite `slug:number`.

## Paging
Site serves 24 card/page; host asks (page, limit=12). Search slices the merged site pages (pola 1manga) dan melaporkan `total`/`pages`; `page` melewati halaman terakhir → site 404 → diperlakukan sebagai halaman kosong (bukan error).

## Gotchas
- Nomor chapter DESIMAL (25.2) — `data-chapter-number` dan id composite harus `[0-9.]+`, jangan `\d+` (23 dari 56 chapter kena buang saat implementasi pertama).
- Lua pattern: href genre mengandung `%5B`/`%5D` — di pattern harus `%%5B`/`%%5D`, else "invalid capture index".
- `call_cached`: edit scraper = `POST /plugins/cache-clear` sebelum retest, else hasil lama.
- cloudflare di depan; UA Chrome + `image_referer` di manifest. rate-limit header x-ratelimit-limit 60.

generated: { by: "kit-agent", at: "2026-11-25" }
