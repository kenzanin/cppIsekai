---
type: Fact
title: "4ucomics plugin: private JSON API v1 behind Cloudflare, thin manga payload"
description: "4ucomics.com /api/v1 is a Cloudflare-gated JSON API proxying MANGADOT; the manga payload has no status and no per-chapter page counts, and slugs are the ids."
tags: [plugins, lua, 4ucomics, api, cloudflare, mangadot]
generated: { by: agent/mcp, at: "2026-11-17T21:50:38Z" }
status: stable
---

## Transport
`https://4ucomics.com/api/v1` only. The HTML shell is a SPA; plain curl gets a Cloudflare 403 "Just a moment" page, so `host.http.fetch(..., { profile = "lightpanda", timeout_ms = 60000 })` is required for every call. The renderer sometimes wraps the JSON in `<pre>...</pre>`; the plugin strips that before `json.decode`.

Images live on per-title CDNs (e.g. `mangadot.net`, itself CF-gated), so every image URL is rewritten through the site's own proxy `/api/v1/img?url=<encoded>`.

## Endpoints
- `/search/<query>?adult=1` -> `{hits:[{id, slug, title, cover, vendor, ...}]}` (30 hits). `id` is numeric, the plugin uses `slug`.
- `/manga/<slug>` -> `{manga:{ id, title, slug, cover, genres, authors, description, type, vendor, statuses, chapters:[{name, rawChapter, date, type}], latest_chapter, recent_chapters, predicted_next_chapter, updated_at }}`.
- `/manga/<slug>?chapter=<name>` -> same payload with `chapters[i].pages[].pageURLs[]` populated for that one chapter.

## Payload gaps (why the plugin emits defaults)
- `statuses` is **empty for every title** (verified on both the manga and the search payload), so status is genuinely `unknown` -- do not invent one from `type`/`latest_chapter`. The detail pill now hides `unknown`.
- Chapters carry no page count; page URLs only arrive per-chapter via `?chapter=`. So `pages = 0` in the chapter list is the house convention (comix and en-thunderscan do the same) and the host's total comes from `chapter_pages` after the pages call, never from the plugin field.
- `authors` is `[]`; there is no artists field; `release_at` has no source date -- the plugin uses the earliest chapter date.
- `description` is empty on most titles.
- Chapter `title` is absent from the chapters array (only `latest_chapter`/`recent_chapters` carry `title`), hence `title = ""`.

## Ids
Title id = the site slug verbatim, which can be **202 characters** -- that is exactly what forced the `valid_title_pair` cap to 255 (see findings/valid-title-pair-charset). Chapter id is the composite `<slug>:<chapter name>`.

# Related Concepts
- [valid_title_pair rejects real-world manga slugs (apostrophes, exclamation marks)](../findings/valid-title-pair-charset.md): 4ucomics' 202-char slug is what forced the valid_title_pair cap to 255.
