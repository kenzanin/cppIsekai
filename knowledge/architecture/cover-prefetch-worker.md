---
type: Decision
title: "cover prefetch: dedicated worker, cache-first, library + search hooks"
description: "Separate reader::CoverWorker (src/reader/cover_worker.hpp) from the page-window reader::Worker: one thread, coalescing queue, woken by library/grid render (saved manga covers) and by /plugin/<id>/search (data.items[].cover). Cache-first by design: img_ensure hit = no fetch; covers land in v3 layout via CoverCtx (same path /cache/img?cover=1 serves) with [cache] image_format transcode. Committed as 23235ca. OpenSpec for the settings page also created this session (validated)."
tags: [cache, cover, worker, prefetch, library, search]
generated: { by: agent/mcp, at: "2026-11-10T10:13:19Z" }
status: stable
---

Split-out 2026-11-10 per user call. reader::CoverWorker mirrors reader::Worker (one thread, no timer, newest list wins) but carries CoverJob{url, plugin} and calls img_ensure(url, CoverCtx{cover=true, plugin, url}, {}). Hooks: (1) home_context in src/http/http_pages.cpp nudges saved manga covers on every library/grid render; (2) register_api /plugin/<id>/<method> for action == "search" parses the envelope (data.items[].cover, incl. stale-served responses) and nudges. Fire-and-forget; search responses never wait on downloads. Search covers land in data/cache/img/<plugin>/cover/<hash16>-<r4>.<ext> just like saved-library covers. Related: architecture/cache-layout-v3, architecture/reader-readahead.
