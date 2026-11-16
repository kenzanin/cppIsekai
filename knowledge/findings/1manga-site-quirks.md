---
type: Finding
title: 1manga.co scraping quirks (HTML port)
description: "1manga.co's GraphQL backend rejects external clients; every feature is reachable by scraping the server-rendered HTML. Records the markup/selector traps the Lua port had to handle."
tags: [plugin, 1manga, scraping, lua, site-quirks]
generated: { by: agent/mcp, at: "2026-11-16T10:53:26Z" }
status: stable
---

## Backend
- `api.mghcdn.com/graphql/mn03` answers 403 `app does not have permission` for every key lifted out of the site client config, so the GraphQL path is unusable for a port.
- Everything is server-rendered HTML: search results (`/search?q=`), genre listings (`/genre/<lower-hyphen-name>`, `/page/N`), detail metadata, the full chapter list, and page images (`imgx.mghcdn.com`, plain GET, no referer needed).
- SPA fetch writes `<!-- -->` between interpolated fragments; strip comments before plain-Lua matching (host.re2 only where a `(?s)` span is required).

## Card markup
- Result card: `<div class="media-manga media">` -> `media-left` (cover `<img src="https://thumb.mghcdn.com/...">`) -> `media-body` -> `<h4 class="media-heading"><a href="https://1manga.co/manga/<slug>"[optional rel]>Title</a><small>by ...</small></h4>`, then `chapters published (Ongoing)`, then `<p class="_12-Zw">` genre row.
- The title anchor sometimes carries `rel="noindex nofollow"`: a pattern that requires `">` right after the href quote skips those cards and lets the match run into the NEXT card, which silently pairs a title with the previous card's cover and halves the result count.
- Thumbnail filenames on the site itself are reused/misnamed (e.g. martial-peak.jpg on several titles); that is upstream data, not a parsing shift.

## Detail / chapters
- Detail: status label, author/artist links, synopsis = the longest `<p>` body without anchors, genres = `a.label.genre-label` anchors. No publication year is exposed, so `release_at` stays 0.
- Chapter rows: `<a href="https://1manga.co/chapter/<slug>/chapter-<n>">` + `#<n>` + optional `- <label>` + `<small>MM-DD-YYYY</small>`.
- Site chapter labels carry scrape noise: `CHapter 3860` and `3854-eng-li`; the plugin drops those and keeps real titles (`- One Year Promise`).

## Genre facet
- The UI genre chips render plain strings and send them back as `tags=`; the plugin maps names to site slugs from the last `genres` fetch.
- `/genre/<slug>` ignores `?q=`, so chip+query is narrowed client-side by title substring.

# Related Concepts
- Related to [goIsekai -> cppIsekai plugin porting matrix](../references/plugin-porting-matrix-goIsekai.md)
