---
type: Fact
title: "keroscans plugin: endpoints + decimal-subchapter gotcha"
description: "keroscans.net plugin: custom theme scrape map (search/browse/detail/chapters partial pager/pages/genres), full-synopsis source, paid-chapter lock marker, 404 end-of-pager signal, decimal subchapters, Lua/manifest gotchas."
tags: [plugins, lua, keroscans, scrape]
generated: { by: agent/mcp, at: "2026-11-25T17:04:12Z" }
status: stable
---

## Site
keroscans.net — custom theme (bukan Madara). Plugin `plugins_dev/lua/keroscans/` (api 1, Lua): search/detail/chapters/pages/genres.

## Endpoints (dari HAR tmp/keroscans.har, live-verified 2026-11-25)
- search: `GET /search?q=&page=N` → `<article data-series-id>` per card; slug `/series/comic/<slug>`, cover `src`, judul `<h3>`. Total di teks `N results`.
- browse (chip genre, q kosong): `GET /series?genre[i]=<slug>&page=N` — `/search` dengan q kosong mengembalikan 0 card. Total di teks `N series`.
- genres: `GET /genres` → chip `<a href="...genre%5B0%5D=<slug>">…<span>Name</span>` (19 genre).
- detail: og:title/og:image + facts `<dt>Author/Artist/Status</dt><dd>`; genres dari JSON-LD `"genre":[...]`. **Deskripsi**: `og:description` DIPOTONG `...`; teks penuh di `<p :class="!open && 'ks-sp__clamp'">` (atribut Alpine `:class`, bukan `class`) → pakai itu, og hanya fallback.
- chapters: page 1 embedded di series page; older = `GET /series/comic/<slug>?page=N&partial=chapters` (wrapper #chapter-list utuh, 10 row/halaman). Lewat halaman terakhir = **HTTP 404** (bukan wrapper kosong) → sinyal berhenti. Row: `data-chapter-number` + `<time datetime=ISO>`; tidak ada judul chapter.
- pages: `GET /series/comic/<slug>/chapter/<n>`; `<img src="/media/pages/..."` urut DOM. Chapter **berbayar** (row `data-access="paid"`) → halaman reader berisi `.ks-reader-locked` dan 0 gambar → plugin balas error "chapter locked (paid)". Chapter id composite `slug:number`.

## Paging
Site serves 24 card/page; host asks (page, limit=12). Search slices the merged site pages (pola 1manga) dan melaporkan `total`/`pages`; `page` melewati halaman terakhir → site 404 → diperlakukan sebagai halaman kosong, dan bila itu site page pertama yang diminta plugin baca ulang site page 1 untuk total yang benar (404 tidak membawa count).

## Gotchas
- Nomor chapter DESIMAL (25.2) — `data-chapter-number` dan id composite harus `[0-9.]+`, jangan `\d+`.
- Lua pattern: href genre mengandung `%5B`/`%5D` — di pattern harus `%%5B`/`%%5D`; attribut `:class` juga aman di-match lewat `[^>]*class="`.
- `host.log` di Lua hanya `info`/`error` (tidak ada `warn`).
- Manifest tidak mengenal key `paginates` (struct `Manifest` cuma punya id/entry/website/site_url/image_referer/image_engine/capabilities/cache_ttl/params/api) — jangan ditambah.
- `call_cached`: edit scraper = `POST /plugins/cache-clear` sebelum retest, else hasil lama (ini bikin fix kelihatan gagal).
- `[cache] image_format = jxl` di ini host: webp dari situs disimpan sebagai JPEG XL (`image/jxl`, ekstensi `.jxl`) — bukan situs yang kirim jxl.
- cloudflare di depan; UA Chrome + `image_referer` di manifest. rate-limit header x-ratelimit-limit 6000 (media), 60 (halaman).

generated: { by: "kit-agent", at: "2026-11-25" }
