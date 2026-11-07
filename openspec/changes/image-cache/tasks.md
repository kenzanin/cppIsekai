# Tasks: image-cache

## 1. Cache core
- [x] 1.1 `src/cache/`: disk store `data/cache/{img,api}`, sha1 keys, LRU index (mem map, rebuild on boot), `cache_max_mb` ini (default 2048), evict-on-insert; unit test evict order with tiny cap
- [x] 1.2 `GET /cache/img?url=` serve-through (content-type by ext; fetch+store on miss online; 404 offline)

## 2. Offline path
- [x] 2.1 `proxy=` ini -> CURLOPT_PROXY on every outbound (plugin fetch + image getter)
- [x] 2.2 stale layer: persist ok responses per `plugin|method|q` on opted read routes; on failure serve last-good + `cached:true`; unit test with failing fetcher

## 3. Mangadex pages + prefetch
- [x] 3.1 `pages` method via `/at-home/server/{id}` (dataSaver off), luac + live probe
- [x] 3.2 `POST /reader/position`, `POST /chapter/<id>/skip` (state `host/skip/<id>`); window pure fn + unit test (boundary spill, skip reroute, no refetch)
- [x] 3.3 worker thread: sequential delta fetch, re-run on position/skip

## 4. Verification
- [x] 4.1 ctest green; openspec valid
- [x] 4.2 live: bogus proxy -> viewed manga detail/chapters/pages `cached:true`, images 200 from disk; unviewed 404; window log shows ch1->ch2 spill and skip reroute
