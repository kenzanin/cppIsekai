---
type: Fact
title: "valid_title_pair rejects real-world manga slugs (apostrophes, exclamation marks)"
description: "valid_title_pair: slug cap raised 128 -> 255 (NAME_MAX, slug is a cache dir name); real 202-char 4ucomics slug was 404'ing as unknown title."
tags: [http, valid_title_pair, slugs, slug-length, name-max, fixed, 4ucomics]
generated: { by: agent/mcp, at: "2026-11-17T21:50:00Z" }
status: stable
---

## Resolution (2026-11-16, implemented)

User direction: host now has unicode support (utf8proc/re2), so the guard should not need ASCII normalization — titles pass as-is. Implemented as a denylist in src/http/api_ctx.hpp `valid_title_pair`: empty/len>128 still refused; `..` still refused; bytes refused: `/` `\` `?` `#` `%` (0x20 space), C0 controls (<0x20) and 0x7f (DEL). Everything else (unicode, apostrophe ! : | etc.) passes. Unit tests extended in tests/test_http_enrich.cpp (sanascans apostrophe cases, unicode, `%` rejected, \ ? # rejected, x7f/x1f rejected).

## Evidence that prompted it

`/api/detail/sanascans/a-contract-marriage-as-the-marquis's-villainous-wife` -> 404 `unknown title` on every endpoint gated by valid_title_pair (detail, refresh, pages, enrichment).

## Still open

Whether `%` should be allowed (currently rejected — no plugin emits raw `%`); revisit if a plugin starts emitting percent-encoded ids directly.


## 2026-11-18: length cap raised 128 -> 255 (NAME_MAX)

The old `len>128` refusal was itself a bug: the 4ucomics plugin uses the site slug as its id, and a real title slug is 202 characters, so `/view/manga/4ucomics/the-strongest-demon-king-at-0-months-old-...` answered `404 {"error":"unknown title"}` on detail/chapters/pages/related/refresh while the page shell still returned 200 (it renders without the slug) -- an empty page, no error text.

`valid_title_pair` (src/http/api_ctx.hpp) now caps at **255**: a slug doubles as a cache directory component (`data/cache/img/<plugin>/manga/<slug>/`), so the real limit is the filesystem's NAME_MAX, not an arbitrary small number. A 256-char slug is still rejected.

The old test had locked the bug in (`CHECK(!valid_title_pair("mangadex", std::string(200,'a')))`); it now requires the real 202-char 4ucomics slug to pass and a 256-char slug to fail (5c19d0a).

Verified live: detail 200, chapters 200 with 11 rows (twice), pages 200, page image 200 `image/jxl`, and the 232-char cache path written under `data/cache/img/4ucomics/manga/`.
