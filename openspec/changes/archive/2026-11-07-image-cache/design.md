# Design: image-cache

## Context

Host already owns all outbound traffic (`http.fetch` for plugins, image URLs flow through
templates today). Backend-first: no reader UI; prefetch is driven by a position endpoint
the future reader (or tests) call.

## Goals / Non-Goals

- Goals: offline-proof reads of opened manga (metadata + images), bounded disk, sliding
  10-page prefetch across chapters with skip.
- Non-Goals: reader UI, library-wide cover preheat, P2P/other tabs, multiuser prefs.

## Decisions

**1. One cache, one LRU pool.** `data/cache/img/<sha>.<ext>` and `data/cache/api/<sha>.json`
share one LRU index (sqlite `cache(path,bytes,atime)` or in-memory map + disk mtime —
in-memory map, rebuilt on boot by scan; atime updated on serve). `cache_max_mb` ini, default
2048; evict least-recently-used whole files on insert overflow. ponytail: single knob; per
class caps when it matters.

**2. Proxy is host-wide and dumb.** `proxy=` ini -> CURLOPT_PROXY on every outbound handle
(plugin fetch + image getter). Bogus host = connect fail = offline. No mode flag: absence
== direct. ponytail: offline ON/OFF later only if bogus-proxy proves annoying.

**3. Stale-serve at the ABI route, keyed `plugin|method|sorted-query`.**
Wrap the call: success -> persist response JSON + bump key; failure (transport or
`ok:false`) -> if a cached entry exists, return it with `"cached":true` added. One wrap
point, no plugin awareness. Reads-only routes opt in (search/detail/chapters/pages/genres).

**4. Chapter order and page lists come from the plugin, cached like everything else.**
Worker resolves the window from `chapters` (number asc) + `pages` (per chapter). mangadex
`pages` uses `/at-home/server/{chapterId}` -> `{host,chapter.hash,chapter.data[]}` ->
`https://{host}/data/{hash}/{file}`. No DB chapter table (ponytail: add when resume
positions need durability beyond cache).

**5. Window math is a pure function.**
`window(chapters, pages_of, position, skipped) -> [urls]`: flatten chapters in order
(skipped excluded), start after current page, take 10. Worker diffs against on-disk
presence, fetches the delta sequentially (polite, 1 at a time), re-runs on every position
or skip change. Testable with injected fake fetcher — no network in unit test.

**6. Skip flags live in `plugin_state` under reserved `host/skip/<chapter_id>` keys.**
Zero schema; `state_set` upsert already exists. ponytail: real `chapter_pref` table when a
second per-chapter pref appears.

**7. Serve-through image route.** `GET /cache/img?url=` -> cached file
(`content-type` from extension), else fetch+store online, 404 offline. Frontends point at
this route so cache is exercised by normal browsing; the worker uses the same core
`ensure(url)`.

## Risks / Trade-offs

- [Worker vs plugin pool] worker calls plugins through the same pool (size 2); prefetch is
  sequential and low rate — acceptable; raise pool if it starves interactive calls
- [atime on every serve] one sqlite-free map update in memory; persistence of recency
  rebuilt from disk mtime on boot — close enough for LRU
- [stale forever] stale-serve has no age limit: offline correctness beats freshness; the
  next successful refresh replaces it

## Migration Plan

1. cache store + `/cache/img` + ini keys; 2. proxy passthrough + stale layer;
3. mangadex `pages`; 4. position/skip + worker + window unit tests; 5. offline sim live
   (bogus proxy, cached manga opens; uncached 404s).
