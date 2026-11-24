---
type: Decision
title: "MangaK + MangaGoku ports, and shared MangaHub engine"
description: "Three site ports from goIsekai extension-source + HARs: mangak.io JSON API, mangagoku.com MangaBox-style site, and mangakalot.fun refactored to a shared MangaHub engine (target later changed to mangagoku.com)."
tags: [plugin, lua, port, har, mangahub]
generated: { by: agent/mcp, at: "2026-11-24T12:24:22Z" }
status: stable
---

## Contract

- `plugins_dev/lua/mangak`: api.mangak.io JSON API (`/titles/search?q=&page=&limit=&genres=`, `/titles/:id`, `/titles/:id/chapters`, `/titles/:id/chapters/:cid`, `/genres`); Accept + same-site Referer headers; pagination from `data.pagination.total/total_pages`; chapter id `<hsid>:<chapterId>`; images from `chapter.images[]`.
- `plugins_dev/lua/mangagoku`: www.mangagoku.com; search `/home/search/json?searchword=&page=` (20/page, memoized bounded binary search for total/pages), detail schema.org Book blob from RSC, chapters `/api/manga/<slug>/chapters?limit=-1`, pages from chapter HTML 2xstorage URLs deduped by numeric index (CDN mirrors duplicate each page).
- `plugins_dev/lib/mangahub.lua`: shared engine moved out of `plugins_dev/lua/mangahub/main.lua`; source id now `host.plugin.param("source_id") or "m01"`. Both mangahub (m01) and the former mangakalot.fun (mn01) entries are thin `require("mangahub")` mains. The mangakalot.fun target was replaced by mangagoku.com per user decision; old dir/symlink removed.

## Why

Reference sources: `goIsekai/examples/plugins/wasm/mangak/main.go` (older API shape), `extension-source/lib-multisrc/mangak`, `extension-source/src/en/mangakakalotfun`, `extension-source/src/en/mangakatana`, and HARs `tmp/mangak.har` / `tmp/mangakalotgg.har`. HAR proved live shapes differ from old Go reference for MangaK (pagination object present) and that mangagoku.com is MangaBox-style, not MangaHub — so the site-specific plugin was rewritten rather than cloned.

## Traps

- RE2 patterns must use `\\[ \\]`, not Lua `%[ %]`; long-bracket delimiters `[==[ ]==]` needed when a pattern contains `]]`.
- First `application/ld+json` blob on mangagoku is Organization, not Book; anchor on `"@type":"Book"`.
- Reader image URLs repeat across CDN mirrors; dedupe by trailing index.

## Verification (2026-11-24)

All methods live through daemon: mangak search/detail/chapters/pages/genres (24 pages for ch12), mangagoku search detail chapters pages genres (61 unique ordered pages, author deduped, status Ongoing), mangahub regression search still 1060 total. `/cache/img` 200 image/jxl for both new plugins (121 KB / 159 KB). Build 0 warnings, suite 21/21.

## MangaGoku reader-page CDN host varies per title
`methods.pages` reads 2xstorage URLs from the chapter HTML (`/manga/<slug>/<chapter>`) and keeps one URL per numeric `/<n>.webp` index. The subdomain is **not stable**: some titles serve `img-r1.2xstorage.com`, others `img-r2`, and others `imgs-2.2xstorage.com` (isekai-yurutto... ch1 is 100 pages, all on imgs-2). The original `img%-r%d+` pattern silently matched nothing for the imgs-2 titles, so the plugin returned `{ok=true, data={}}` and the host 404'd with a bare `unknown chapter` (empty pages look the same as a missing chapter to `build_pages_response`). Fixed 2026-11-24: pattern is `https://[%w%-]+%.2xstorage%.com/[^"\\%s]+/%d+%.webp`; the numeric-tail gate still excludes `/thumb/` covers. Verified live: isekai-yurutto 100/100 pages + `/cache/img` 200 image/jxl on pages 1/51/100; solo-leveling unchanged at 55 pages on img-r1.

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md): new Lua ports following the folder/manifest/JSON-text ABI
- [Image referer follows the URL's own site domain](../decisions/image-referer-domain-match.md): mangak.io and mangagoku.com image CDNs use manifest image_referer / same-site referer
