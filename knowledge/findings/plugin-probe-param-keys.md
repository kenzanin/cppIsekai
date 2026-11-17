---
type: Finding
title: Plugin probes with wrong param keys write a cache row that shadows the real handler
description: "call_cached stores whatever a probe returns, so probing a plugin with slug= instead of id= cached an empty envelope that then served every real request."
tags: [plugins, cache, api_cache, probe, call_cached, debugging, en-thunderscan]
generated: { by: agent/mcp, at: "2026-11-17T21:50:39Z" }
status: stable
---

## What happened
Diagnosing an en-thunderscan title that showed "1 chapter" when the site had more, a probe called the plugin as `/plugin/en-thunderscan/chapters?plugin=en-thunderscan&slug=...`. The plugin ABI wants `id=` (Lua reads `plugin.param("id")`), so the handler got nothing and returned `{"ok":true,"data":{}}` -- 11 bytes.

`call_cached` wrote that 11-byte row to `api_cache`. Because cache rows are served at any age (memory #5427), **every later request, including the UI's own**, received the empty envelope. The scrape was fine the whole time; the cache was lying, and it was my own probe that put it there.

## Rule
1. Before concluding "the plugin returns nothing", check the cache: `SELECT params_json, length(body) FROM api_cache WHERE plugin='<id>'` -- a tiny body is the tell.
2. Purge by `params_json` after any response-shape change or bad probe: memory #5426.
3. Quote the plugin's own `main.json` / `plugin.param(...)` keys in a probe; the ABI is not uniform -- most plugins take `id`, some `slug`, comix-style ones take title ids, and a wrong key fails silently as an empty envelope, never as an error.

# Related Concepts
- [manga-detail-page: stale-serve + fail-safe refresh pattern](../architecture/manga-detail-page.md): call_cached serves rows at any age, so a bad probe's cache row behaves like a stale-serve hit.
