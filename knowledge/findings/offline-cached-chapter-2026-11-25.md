---
type: Finding
title: "Offline cached chapters: /api/pages must fall back to chapter rows, not the plugin envelope"
description: "Verified by setting [server] proxy=http://127.0.0.1:9 (all live plugin fetches -> 'Couldn't connect to server'): /api/pages/mangafire/w5r76/31 still returned 16 pages, prev 30/next 32, /cache/img served the cached jxl (200, 191 KB), and the real reader rendered 16/16 images with no errors. Before the fix the route resolved the chapter solely from call_cached('chapters') and call_cached('pages'), so an expired/missing envelope returned 'unknown chapter' offline even though chapters + chapter_pages rows and image files were all on disk. Fix: rebuild a chapters envelope from Store::chapters_list when resolution fails, and feed stored chapter_pages URLs (new Store::chapter_pages_urls, OpTrace-serialized) into build_pages_response when the pages call fails. Proxy was restored afterwards; live fetch green again."
tags: [offline, cache, chapter-pages, store, reader, fixed]
generated: { by: agent/mcp, at: "2026-11-25T01:41:08Z" }
status: stable
---

# Related Concepts
- Related to [Chapter state: one flags bitfield per chapter row, chapters as metadata hub](../architecture/chapter-state-flags.md)
