---
type: Finding
title: "mangahub.io GraphQL moved to the site host, cookie auth, NSFW flag"
description: "MangaHub's GraphQL API left api.mghcdn.com for mangahub.io/graphql, authenticates on the mhub_access cookie (header auth now 403s), toggles NSFW via hideNSFW:false, and pages 30 per request."
tags: [plugin, mangahub, graphql, nsfw]
generated: { by: agent/mcp, at: "2026-11-19T14:56:03Z" }
status: stable
---

# MangaHub GraphQL 2026-11 move

Evidence: `tmp/mangahub.har` (browser session, 2026-11-19) plus live curl probes.

## What changed
- Endpoint: `https://api.mghcdn.com/graphql` -> `https://mangahub.io/graphql`.
  The old host answers **403** for every query, which the plugin surfaced as
  "search: found 0 results" (the refusal was logged as `GraphQL status 403`).
- Auth: the browser sends only `Origin`/`Referer` plus the `mhub_access`
  cookie. The `x-mhub-access` header the plugin used is no longer accepted on
  its own. The plugin now sends `Cookie: mhub_access=<key>; showNSFW=1`.
- Key minting is unchanged: `GET /chapter/<slug>/chapter-<n>?reloadKey=1` with
  a fabricated `mhub_access` cookie returns a fresh key in `Set-Cookie`.
- Paging: search is offset-based, page size **30**. `count: true` adds a
  `count` field with the real total (e.g. q=dungeon -> 393), so the pager can
  report real page counts instead of a single page.
- NSFW: the site's "show NSFW content" toggle is the `hideNSFW` query
  argument (`hideNSFW:false` when enabled, together with the `showNSFW=1`
  cookie). cppIsekai sends `hideNSFW:false` so adult titles are searchable.

## Query shapes that work (verified)
```
{search(x:m01,q:"dungeon",genre:"all",mod:POPULAR,hideNSFW:false,count:true,offset:0){rows{title,slug,image},count}}
{manga(x:m01,slug:"dungeon-busters"){title,slug,status,image,author,artist,genres,description,alternativeTitle,chapters{number,title,date}}}
{chapter(x:m01,slug:"dungeon-busters",number:1){pages,mangaID,number}}
{genres{id,slug,title}}
```

`genres` replaces the old scrape of `/genre/<slug>` anchors on `/search`
(markup drift made the chip list silently empty).

## Float ids
Chapter `number` arrives as a JSON float, so `tostring(1)` is `"1.0"` and
chapter ids were minted as `<slug>:chapter-1.0`. Integral numbers are now
formatted with `string.format("%d", ...)`, and `methods.pages` strips a
legacy `.0` suffix so old cached reader URLs keep working.

## Verification
- search: dungeon 30/393 (14 pages), one piece 30/1051 (36), sex 30/318 (11),
  pornstar 4/4 (NSFW titles present), page 2 differs, genre=Adult 3957.
- chapters: 37 rows, newest-first, `dungeon-busters:chapter-37`, dates parsed.
- pages: 35 decrypted imgx URLs; `/api/pages/mangahub/dungeon-busters/1` ->
  `/cache/img` 200 image/jxl 269 KB.
- UI: headless Chrome on `/search?plugin=mangahub&q=dungeon` renders 30 cards
  and the pager "393 results ... 1 ... 2 ... 14".
