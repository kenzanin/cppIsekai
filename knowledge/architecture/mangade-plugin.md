---
type: Fact
title: "MangaDE Lua plugin: JSON API contract from tmp/mangade.har"
description: "Plugin id mangade, ported from goIsekai src/en/mangade + tmp/mangade.har. api.mangade.io/api requires Referer+Origin https://mangade.io/; GET /comics?page&size=20&sort=newest&name= (data.list/totalCount/totalPage, genre filters genres[0]=id), /comics/<id>/view (status, genre_names CSV, news_chapters), /chapters/<chapterId>/view (chapter_images[].image), /genres?size=500 (129 names). Composite chapter id <comicId>:<chapterId>; cover/page images on s3-load.ttr.group with site referer. Lua plugin, no crypto/cookies. Verified 20 search items for isekai (total 91/5 pages), detail, 7 chapters, 31 pages, /cache/img 200 jxl for cover and page, 20 browser cards and 7 rendered chapter rows."
tags: [plugin, lua, mangade, json-api, har]
generated: { by: agent/mcp, at: "2026-11-24T16:16:41Z" }
status: stable
---

# Related Concepts
- Related to [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md)
