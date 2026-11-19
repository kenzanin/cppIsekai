---
type: Constraint
title: "DLE sites: JSON numeric ids must be formatted integrally, and image CDNs need the trailing-slash Referer"
description: "Lua JSON decodes site ids as floats, so tostring renders /reader/15840.0/991476.0 and mangabay's reader 404s (host surfaces it as 'unknown chapter'); komacdn additionally 403s without Referer https://manga-bay.biz/."
tags: [plugins, lua, mangabay, batcave, dle, ids, image-referer, reader]
generated: { by: agent/mcp, at: "2026-11-19T07:24:00Z" }
status: stable
---

# DLE JSON float ids + image Referer

## Symptom
GET /api/pages/mangabay/<slug>/1 -> 404 {"ok":false,"error":"unknown chapter"} while /api/chapters returned a full list.

## Root cause 1 - float ids in the reader path (mangabay)
- The chapter entries come from the page's window.__DATA__ JSON, and json.decode yields Lua floats.
  tostring(c.id) / tostring(data.news_id) therefore render 991476.0 / 15840.0.
- Built reader path: /reader/15840.0/991476.0 -> the site answers its 1196-byte 404 page,
  dle.json_blob finds no __DATA__, so methods.pages returns an empty page list
  and build_pages_response returns nullopt -> "unknown chapter" (the host prints the 404,
  the plugin error only reaches the daemon log).
- Fix: `num_str(v)` in plugins_dev/lua/mangabay/main.lua formats integral floats with
  string.format("%d", v); chapters builds /reader/<num_str(news_id)>/<num_str(c.id)>.
  methods.pages also normalises incoming ids (%.0$ -> "", %.0/ -> "/") so reader tabs
  that still hold the old float id keep working.
- mangafire already did this with string.format("%.0f", cid) - the same defect class.

## Root cause 2 - image Referer (komacdn)
- Every page image 403'd: komacdn requires Referer https://manga-bay.biz/ (trailing slash).
  Bare origin, no referer, and komacdn.com referers all return 403 (4545-byte HTML).
- Host default referer is the manifest `website` origin, which mangabay did not set, so no
  Referer was sent. Fix: "image_referer": "https://manga-bay.biz/" in main.json
  (same class as decisions/search-envelope-page-count / the mangaread trailing-slash finding,
  see findings/js-image-referer-gap).

## Verified (2026-11-19)
- /api/chapters ids are now /reader/15840/991476 ... 111 chapters.
- /api/pages ch1 -> 38 pages, ch_id /reader/15840/991476, next 2.
- /cache/img page 1/6/38 -> 200 image/jxl.
- Live browser (blueprint-mcp): ch1 38 images 38 loaded 0 broken; ch2 29/29 loaded.
- 497 stale float-id chapter rows (flags 0) deleted from the store.
- Suite 19/19.

## Note
batcave builds the same float ids (/reader/4715.0/24313.0) but its reader route tolerates
them (fresh fetch + 200 jxl verified), so it is left as-is. Any plugin that stringifies a
JSON number into an id or a URL must format it integrally.

# Related Concepts
- Related to [Host image fetch is plugin-blind: no Referer (js mangatown pilot finding)](../findings/js-image-referer-gap.md)
- Related to [Plugin page-image regexes must match every image extension a site serves](plugin-image-url-regex.md)
