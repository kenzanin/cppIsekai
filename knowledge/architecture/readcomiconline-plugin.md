---
type: Decision
title: "Read Comics Online plugin: HAR-derived Blade scraper + bound CF clearance"
description: "Lua plugin for readcomicsonline.ru built from tmp/readcomiconline.har: server-rendered HTML, all-results search, reader-all images, Cloudflare clearance cookie+UA in manifest params."
tags: [plugin, lua, cloudflare, har, scraper]
generated: { by: agent/mcp, at: "2026-11-24T01:15:49Z" }
status: stable
---

## Contract

- Plugin `readcomiconline` (id from folder) under `plugins_dev/lua/`, symlinked into `data/plugins/`. Manifest declares capabilities search/detail/chapters/pages/genres and params `site_url`, `user_agent`, `cookie`.
- `GET /advanced-search?name=<q>&status_id=&type_id=&category=<id>` returns ALL results (60 for "batman"); no pager. `categories()` memoizes the site's numeric publisher map from the form's `<select name="category">`; the first selected tag name maps to a numeric id.
- Detail `/comic/<slug>`: h1 name, cover from the `w-full rounded-xl` img, status from the emerald pill, genres from `/comic-list/category/<slug>` links, synopsis from the Synopsis block. Chapters are `<a href=…/comic/<slug>/N>` rows, canonical number ascending, date via host.text.date_epoch.
- Pages: `<div id="reader-all">` holds every page image; id `slug:number`. CDN images resolve their Referer through the host's domain match (cdn.readcomicsonline.ru ↔ readcomicsonline.ru).

## Why

Cloudflare challenges plain curl-impersonate profiles AND lightpanda's default UA, and Chrome CDP navigation to this host times out. The HAR contains a valid `cf_clearance` + session cookie bound to the captured Chrome/158 UA; replaying that pair with plain curl returns 200 (60 cards). Keeping them in manifest params lets operators refresh via file edit + restart, no host code change.

## Traps paid for

- Lua-style `%.` / `%-escapes in host.re2 (RE2) patterns silently match nothing — use `\.` / plain `-`; only Lua's own string.match uses `%`.
- `host.json` does not exist; the JSON library is the global `json` table, and `host.log` has no `warn` (only error/info).
- Plain HTTP without the bound cookie/UA = CF 403; lightpanda fetch without the exact wait flags also 403s (rate-limited after repeated probes).

## Verification (2026-11-24)

All methods live: search batman 60 items, detail 9 fields, 4 chapters, 31 pages for ch1, 82 genres. Host routes `/api/detail`, `/api/chapters`, `/api/pages`, `/cache/img` (cover 10 KB jxl, page 216 KB jxl) 200. Real browser: search 60 cards, detail h1 + 20 chapter rows + cover 250px, reader 45 pages with 2 windowed images at 1988px. Suite 21/21.

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md): new Lua port following the folder/manifest/JSON-text ABI; no runtime changes
- [Image referer follows the URL's own site domain](../decisions/image-referer-domain-match.md): CDN images rely on host domain-matched Referer, no manifest override
