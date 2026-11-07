# image-cache delta

## ADDED Requirements

### Requirement: Disk cache with LRU cap
The host SHALL cache image bodies under `data/cache/img/` and plugin JSON responses under
`data/cache/api/`, one shared LRU pool capped by `cache_max_mb` (ini, default 2048); on
overflow the least-recently-used files are evicted. Cache presence and recency survive
restart (index rebuilt from disk).

#### Scenario: Insert evicts oldest
- **WHEN** the pool is at `cache_max_mb` and a new image is stored
- **THEN** the least-recently-used cached file is deleted and the new file is present

#### Scenario: Restart keeps cache
- **WHEN** the host restarts with populated `data/cache/`
- **THEN** previously cached images still serve and count toward the cap

### Requirement: Serve-through image route
`GET /cache/img?url=` SHALL return the cached file with its content type; on miss online it
fetches, stores, then serves; on miss offline it returns 404.

#### Scenario: Second view is local
- **WHEN** the same image URL is requested twice with the network up
- **THEN** the first request populates the cache and the second is served from disk

### Requirement: Offline via proxy option
The ini `proxy` value SHALL be used as proxy for every outbound request (plugin
`http.fetch` and image fetch); an unreachable proxy makes all live calls fail while cached
data remains served.

#### Scenario: Cached manga opens offline
- **WHEN** `proxy` points at an unreachable host and a manga whose pages/chapters were
  previously viewed is opened
- **THEN** detail, chapter list, and cached page images all render from cache, each
  response marked `cached:true` where served from the stale layer

#### Scenario: Uncached offline miss
- **WHEN** offline and a never-viewed page is requested
- **THEN** the image route answers 404 and the API route answers its failure shape

### Requirement: Stale-serve on failure
For opted-in read routes the host SHALL persist every successful plugin response keyed by
plugin+method+sorted query, and on any later failure (`ok:false` or transport) SHALL return
the last-good entry with `cached:true` when one exists.

#### Scenario: Upstream down, cache answers
- **WHEN** the plugin call fails and a previous identical query succeeded
- **THEN** the route returns the previous payload with `cached:true`

### Requirement: Sliding prefetch window
`POST /reader/position` `{"chapter": <id>, "page": <n>}` SHALL (re)compute the next 10
unread pages: chapters in ascending order, skipped chapters excluded, starting after the
current page, crossing chapter boundaries; the worker caches exactly the not-yet-cached
ones. `POST /chapter/<id>/skip` `{"skipped": bool}` SHALL persist the flag and re-run the
window.

#### Scenario: Boundary spill
- **WHEN** position is the last page of ch1 and ch2 is not skipped
- **THEN** the remaining window pages come from the start of ch2

#### Scenario: Skip reroutes the window
- **WHEN** the same position holds and ch2 is then marked skipped
- **THEN** the window pages now come from ch3 and no ch2 pages are fetched

#### Scenario: Already-cached pages are not refetched
- **WHEN** the window is recomputed after page turn
- **THEN** only pages absent on disk are downloaded
