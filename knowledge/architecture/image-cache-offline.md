---
type: decision
title: cppIsekai image-cache + offline-first pathway
description: "Decision: host-side disk-LRU cache (data/cache/{img,api}, sha1 keys, cache_max_mb ini 2048MB, mem-index+mtime rebuild) + proxytigen host-wide bogus proxy offline sim; /cache/img serve-through; stale-serve last-good +cached:true di satu wrap point ABI (optin search/detail/chapters/pages/genres); mangadex pages via /at-home/server/{id} data[]; reader position/skip snprintf ke host/skip/<id> state; sliding 10-page window pure fn; worker header-only nudge-only sequential delta fetch; live probe verified offline. Task 1.1-4.2, Subagent-driven with per-task reviews."
tags: [rewrite, cache, offline, proxy, stale, reader, mangadex]
generated: { by: agent/mcp, at: "2026-11-07T09:47:03Z" }
status: stable
---


