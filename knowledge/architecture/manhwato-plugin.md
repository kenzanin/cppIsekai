---
type: Fact
title: "Manhwato Lua plugin: SSR HTML via the daemon's Chrome bridge"
description: "Plugin id manhwato, ported from tmp/manhwato.har + goIsekai wasm example. All fetches go through host.browser (goto + evaluate document.outerHTML): the linked curl-impersonate/BoringSSL rejects this site's TLS chain, while system OpenSSL cannot be used in the same process (chrome+system libcurl segfault in curl_multi_poll; ff155 profile crashed the daemon). Search GET /search?q=&page=N (30 cards, pager from page 1), genre chips /manhwa-genre/<slug>, detail /manhwa/<slug> with 98 chapters from the mt-clist, pages from figure.mt-page img, genres from /manhwa-list (118). Verified search 56/2 pages with distinct page 2, detail name+6 genres+440-char synopsis, 98 chapters, 35 page URLs, /cache/img 200 jxl for cover and page, browser 30 cards + 20 chapter rows."
tags: [plugin, lua, manhwato, host.browser, har]
generated: { by: agent/mcp, at: "2026-11-24T23:10:17Z" }
status: stable
---

# Related Concepts
- Related to [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md)
