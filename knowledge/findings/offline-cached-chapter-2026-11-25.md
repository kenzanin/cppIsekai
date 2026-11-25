---
type: Finding
title: "Offline cached chapters: /api/pages must fall back to chapter rows, not the plugin envelope"
description: "With [server] proxy=http://127.0.0.1:9 (all plugin fetches fail) /api/pages/mangafire/w5r76/31 still returned 16 pages, prev 30/next 32, /cache/img 200 jxl, reader 16/16 images. Root cause: /api/pages resolved chapters+page URLs only from call_cached('chapters')/'pages', so an expired envelope gave 'unknown chapter' offline despite DB rows and jxl files. Fix: rebuild chapters envelope from Store::chapters_list; use stored chapter_pages URLs via new Store::chapter_pages_urls (OpTrace). Proxy restored. Offline envelope shapes are now shared pages_api.hpp helpers used by route and tests; test_pages asserts ch_id/prev/next/encoded URLs + negative case, test_store asserts the getter. Suite 22/22. Route-level fallback selection is live-proven (dead proxy), not unit-tested."
tags: [offline, cache, chapter-pages, store, reader, fixed]
generated: { by: agent/mcp, at: "2026-11-25T02:18:05Z" }
status: stable
---

# Related Concepts
- Related to [Chapter state: one flags bitfield per chapter row, chapters as metadata hub](../architecture/chapter-state-flags.md)
