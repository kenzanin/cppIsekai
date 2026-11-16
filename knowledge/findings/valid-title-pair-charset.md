---
type: Fact
title: "valid_title_pair rejects real-world manga slugs (apostrophes, exclamation marks)"
description: "valid_title_pair whitelist blocked real slugs; replaced 2026-11-16 with a denylist (reject / \\ ? # %, space, C0/DEL, dot segments) — unicode and punctuation pass."
tags: [http, valid_title_pair, slugs, open-decision]
generated: { by: agent/mcp, at: "2026-11-16T07:13:46Z" }
status: stable
---

## Resolution (2026-11-16, implemented)

User direction: host now has unicode support (utf8proc/re2), so the guard should not need ASCII normalization — titles pass as-is. Implemented as a denylist in src/http/api_ctx.hpp `valid_title_pair`: empty/len>128 still refused; `..` still refused; bytes refused: `/` `\` `?` `#` `%` (0x20 space), C0 controls (<0x20) and 0x7f (DEL). Everything else (unicode, apostrophe ! : | etc.) passes. Unit tests extended in tests/test_http_enrich.cpp (sanascans apostrophe cases, unicode, `%` rejected, \ ? # rejected, x7f/x1f rejected).

## Evidence that prompted it

`/api/detail/sanascans/a-contract-marriage-as-the-marquis's-villainous-wife` -> 404 `unknown title` on every endpoint gated by valid_title_pair (detail, refresh, pages, enrichment).

## Still open

Whether `%` should be allowed (currently rejected — no plugin emits raw `%`); revisit if a plugin starts emitting percent-encoded ids directly.
