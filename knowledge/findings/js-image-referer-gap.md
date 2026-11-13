---
type: Fact
title: "Host image fetch is plugin-blind: no Referer (js mangatown pilot finding)"
description: Image CDNs that require Referer 403 through the host cache/reader path; fix = per-plugin image-referer plumbed to the img fetcher.
tags: [image-cache, fetch, js-runtime, mangatown, followup]
generated: { by: agent/mcp, at: "2026-11-13T15:04:51Z" }
status: stable
---

Found while porting mangatown to the JS runtime (pilot 2.1/2.2, commit 39dbed4).

- `zjcdn.mangahere.org` images 403 without a Referer (`https://m.mangatown.com/` or www) — UA alone is not enough (probe: ua-only 403, referer-only 200).
- Host image path (`cache::img_ensure` -> `fetch_` functor) is plugin-blind: functor is `(url)` only, no plugin ctx, so no per-site Referer can be attached today.
- `fetch::Request` already has a `headers` vector — only the plumbing is missing (per-plugin referer, e.g. manifest field -> img-fetch functor receiving plugin id from CoverCtx/PageCtx).
- Lua plugins are equally affected; mangafire/mangadex just don't need Referer.
- The JS pages contract is plain URL strings (reader `http_api_reader.cpp` keeps string elements only), so headers cannot ride on page entries.

Followup shape: manifest `image_referer` (or plugin config) + fetch functor signature `(url, plugin)`; verify with `GET /cache/img` on a zjcen URL.
