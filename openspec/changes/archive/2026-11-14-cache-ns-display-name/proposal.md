# Proposal: cache-ns-display-name

## Why

The cache directory component under `img/` is the raw plugin **id**
(`img/<plugin-id>/manga/<slug>/...`). The id is a folder/manifest detail: renaming a plugin
folder, re-installing it under another id, or shipping it as a new plugin version under a new
id throws away its entire cached cover and page library, and the images have to be fetched
again from the source site.

What actually identifies a site to its users — and what stays stable across a re-install — is
the plugin's **display name**. Keying the cache on the name makes the cached files survive an
id change, and makes the on-disk tree readable (`img/mangadex/manga/...` today versus
`img/<name-slug>/manga/...`).

The costs are bounded and one-off, see the design: an existing cache is keyed by id, so the
first boot after this change must discard it (a cold image cache), and two plugins sharing a
display name would share a cache directory.

## What Changes

- Add `cache::cache_ns(plugin_id, name)` — the slugified display name, falling back to the id
  when the name is blank or slugifies to nothing. This is the single definition of the
  namespace.
- Give `Cache` an injectable id → namespace resolver (`Cache::NsResolver`) plus
  `Cache::ns(plugin)`, so the cache keys every `img/` path it writes or looks up on the
  namespace. `Cache` keeps no dependency on the Store or the plugin registry; with no resolver
  injected the id is the namespace, exactly as before.
- Route the two path builders that live outside `Cache` (`/api/cbz-ready` probe and
  `/export/cbz`) through `Cache::ns()` instead of concatenating the raw id.
- Wire the resolver in `main.cpp` from `store.plugins()` at boot.
- Discard the now-stale id-keyed `img/` tree once, behind a `.cache-ns-name` boot stamp.
- Delete the img pool's context-free sha fallback: a `/cache/img` request with no cover flag and
  no complete page context becomes a 404 that fetches nothing, and the prefetch worker drops a
  nudge without placement context. The `api/<sha>.json` pool keeps its sha naming.
- Name the namespace in the `cache-layout` spec, replacing `<plugin>` with the display-name
  namespace, and record the collision/stability rules.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `cache-layout`: the `img/` directory component is the plugin display-name namespace, not the
  plugin id; boot-time cutover from id-keyed trees.

## Impact

- `src/cache/slug.hpp` — `cache_ns` helper.
- `src/cache/cache.hpp`, `src/cache/cache.cpp` — `NsResolver`, `Cache::ns`, one-time cutover.
- `src/cache/cache_img.cpp` — page/cover rel paths and the extension-free hit lookup.
- `src/http/http_api_content.cpp` — `chapter_cached` probe and CBZ export pass the namespace.
- `src/main.cpp` — resolver snapshot from `store.plugins()`.
- `tests/test_slug.cpp`, `tests/test_cache.cpp`, `tests/test_http_img.cpp`,
  `tests/test_worker.cpp` — helper, on-disk path, and context-free-refusal tests.
- Behaviour change: one cold cache on the first boot after upgrade; `img/<id>/` directories are
  no longer created; `/cache/img?url=` without context is a 404 instead of a sha-fallback serve.
