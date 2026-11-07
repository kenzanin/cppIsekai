# Proposal: image-cache

## Why

cppIsekai must be local-first: with the network unreachable (simulated by a bogus `proxy`
in `cppisekai.ini`), an already-opened manga stays fully usable — metadata, chapter list,
and page images — from cache. Today every view hits the plugin live, images are hot-linked
to CDNs, and nothing survives offline.

## What Changes

- Host-side disk cache (`data/cache/`) for images AND plugin JSON responses, single LRU
  pool capped by `cache_max_mb` (default 2048).
- `proxy=` host option applied to ALL outbound curl (plugin `http.fetch` + image fetcher);
  unreachable proxy == offline simulation. On any live failure, the ABI route serves the
  last-good cached response for that plugin+method+query (stale-serve).
- New mangadex `pages` method: chapter id -> page image URLs (`/at-home/server/{id}`).
- Reader-independent prefetch API: `POST /reader/position {chapter, page}` and
  `POST /chapter/<id>/skip`; a background worker keeps a sliding window of the next 10
  unread pages cached, crossing chapter boundaries and honouring skipped chapters
  (1 page left in ch1 -> 9 pages of ch2; ch2 skipped -> 9 from ch3).
- `GET /cache/img?url=` serve-through: disk hit, else fetch+store (online) / 404 (offline).

## Capabilities

### New Capabilities
- `image-cache` — disk store + LRU cap, stale-serve, position/skip, prefetch worker, local serving.

### Modified Capabilities
- `mangadex-plugin` — `pages` method.

## Impact

- New `src/cache/` (store + worker), `src/http/http.cpp` routes, `src/plugin/plugin.cpp`
  (proxy passthrough, stale layer), `plugins_dev/lua/mangadex/main.lua` (`pages`).
- `data/cppisekai.ini`: `proxy`, `cache_max_mb`.
- Store: skip flags under reserved `host/skip/<chapter_id>` keys in `plugin_state` —
  no schema change.
- Tests: cache LRU unit, window math (injectable fetcher), stale-serve; live offline sim.
