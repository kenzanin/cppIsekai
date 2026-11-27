---
type: Decision
title: "Page lists are read from chapter_pages; api_cache no longer stores pages (openspec pages-local-first)"
description: "IMPLEMENTED (branch pages-local-first, 1c1d64e + archive 884d2d5): GET /api/pages answers from chapter_pages rows whenever the release has them (no plugin call, no 100%-on-disk gate); a locally served chapter that is not complete on disk schedules one silent re-parse (plugin pool, once per release per process, disabled plugin stays quiet); reader_window reads the same rows; `pages` left stale_optin and the 30-minute TTL special case is gone."
tags: [cache, offline, api_cache, chapter-store, reader-page, local-first, openspec, local-first]
generated: { by: agent/mcp, at: "2026-11-27T09:05:00Z" }
status: stable
---

## Decision

`chapter_pages` (`plugin, slug, chapter_id, idx, url`) is the durable page list and the only read
source for a chapter that has ever been opened. `api_cache` is the disposable envelope store, so a
second `pages` copy is dead weight: it is gone from `stale_optin`, and `cache_ttl_for` (whose only
job was the 30-minute page TTL) is deleted.

- `GET /api/pages/<plugin>/<slug>/<ch>` resolves the release from `chapters` rows and answers from
  `chapter_pages_urls` on a hit; `call_cached` runs only for a release with no rows (never opened).
- Disk completeness is not a serve gate: page cache files are keyed `img/<ns>/manga/<slug>/chNNN/pNNN`,
  never by URL, so only a *missing* page image needs the network.
- Silent revalidation (option C): a local serve whose chapter is not fully on disk submits one
  `plugin_pool.detach_task` re-parse -> `chapter_pages_replace`. Throttle is a process-lifetime
  `(plugin|slug|chapter_id)` set; the task goes through `call_cached` and is only scheduled while the
  plugin is enabled. Accepted: the triggering open may still use a decayed URL, so a not-yet-cached
  page can 404 once (comico `d=`, mangatown token/ttl).
- `reader_window` composes chapters + page lists from the store too, so prefetch on a stored title
  makes no metadata request and fetches only missing page images.
- Supersedes `library-promote-on-save` D6 for `pages` only; detail/chapters keep their `api_cache`
  behavior for titles outside the library.

Measured smoke: with the plugin disabled, opening a fully cached chapter served 27 pages from rows,
10 image hits, 0 image fetches, 0 plugin parses. Cold chapter ch73: one parse, 21 rows written, zero
`api_cache` rows.

# Related Concepts
- [Library promotion: api_cache is the staging store](library-promote-on-save.md): D6 kept `pages` in
  api_cache; this change reverses that for page lists only.
- [Local-first reads](../architecture/local-first-reads.md): the read model this extends to the reader.
- [cppIsekai image-cache + offline-first pathway](../architecture/image-cache-offline.md): the disk
  layout the page-key rule depends on.
