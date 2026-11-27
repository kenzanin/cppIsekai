# Proposal: pages-local-first

## Why

`chapter_pages` already holds every page URL a chapter ever exposed (12k rows on this machine), and
survives restart, prune and refresh. The reader does not use it: `GET /api/pages` still goes through
`call_cached`, so a chapter whose `api_cache` row is gone — reaped after `fetched_at + ttl + 7d`, or
wiped by plugin prune / unsave — asks the site again even when every page image of that chapter is
already on disk. `chapter_pages` is only consulted *after* the plugin call fails.

The image files themselves do not need the URL: page cache files are keyed
`img/<ns>/manga/<slug>/chNNN/pNNN.<ext>`, not by URL. So for a chapter that has been opened once, the
route has everything it needs in sqlite and the disk, and the network is only legitimately needed for
a page image that is not on disk yet, or for a chapter that was never opened.

## What Changes

- **Page lists are served from `chapter_pages`.** When rows exist for the target release,
  `GET /api/pages` answers from them and never calls the plugin. Cold miss (never-opened chapter)
  keeps the current cached-or-live path and still writes the rows on success.
- **Silent revalidation.** When the chapter is served locally but not fully on disk, the route
  schedules one background re-parse of that chapter's page list (plugin pool, fire-and-forget,
  throttled once per chapter per process) so signed/expired URLs are refreshed for the next open.
- **The reader window reads rows too.** `reader_window` (prefetch input) resolves chapters and page
  lists from the store instead of `call_cached`, so a cached chapter's prefetch costs no metadata
  request. Prefetch keeps fetching page *images* that are missing on disk.
- **`pages` leaves the stale layer.** It is dropped from `stale_optin` and the 30-minute TTL special
  case disappears; the page list's durable copy is `chapter_pages`, so a second `api_cache` copy
  would only duplicate it.

Out of scope: distinguishing offline from an expired-URL 4xx inside `Cache::Fetcher` (a page whose
signed URL decayed can still 404 on the first open after decay), durable detail rows for titles that
are not in the library, and cover URLs whose hash changes because the site signs them.

## Impact

- Specs: `reader-page`, `chapter-store`, `search-cache`.
- No schema change, no plugin ABI change, no new dependency.
- Accepted consequence: the open that triggers a silent revalidation still renders the URLs it had;
  a not-yet-cached page of that chapter can 404 once and works from the next open.
