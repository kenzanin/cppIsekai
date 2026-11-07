# Proposal: cache-layout

## Why

The image cache today is opaque: `data/cache/img/<sha>`. You cannot tell which manga a
file belongs to, chapters cannot be zipped for reading elsewhere, and nothing about the
disk layout invites export. The OKF decision (architecture/image-cache-offline, layout
v2) already fixes the layout on paper; this change implements it: a readable, CBZ-ready
per-plugin tree with a self-validating export route.

## What Changes

- Storage layout v2 (per plugin):
  - `data/cache/<plugin>/covers/<manga-slug>.<ext>` — overwrite on refetch
  - `data/cache/<plugin>/<manga-slug>/ch<NNN>/p<NNN>.<ext>` — zero padded
  - `data/cache/api/<sha>.json` — unchanged (stale layer)
  - `data/cache/img/<sha>.<ext>` — kept only as fallback for context-free `GET /cache/img`
- Slug: lowercase fs-safe, hashed in hash-library on collision (no DB).
- Evictor walks the whole `data/cache/` recursively; no more scan by pool dir.
- Existing packs (`data/cache/{img,api}/`) purged once on first boot after this change;
  no migration code.
- Reader window worker writes into the structured layout (plugin/slug/ch/page) instead
  of sha; `/cache/img` still works as sha fallback.
- New route `GET /export/cbz?plugin=<id>&slug=<manga-id>` — streams a CBZ (zip) of the
  cached cover + chapter pages, lexicographically sorted (zero-pad makes it reading
  order).

## Capabilities

### New Capabilities
- `cache-layout` — per-plugin structured cache layout with slug addressing + LRU
  eviction walker over mixed layout tree.

### Modified Capabilities
- `mangadex-plugin` — cover/images now fetched into structured layout keyed by slug.
- `image-cache` — layout becomes v2 (per plugin/slug/chapter/page) with fallback.

## Impact

- New `src/cache/slug.hpp` (fs-safe slug, unique-by-suffix); `src/cache/cache.cpp` path
  resolution + recursive evict walk; `src/reader/worker.hpp` placement args threading;
  `src/http/http.cpp` +1 route `/export/cbz`; `_ migrant: purge first boot`.
- Zero DB schema change; zero new dependency (slug = local, export = reuse of libzippp
  pending user K2 — or vendored minizip fallback if libzippp usage stalls).
- Tests: slug unit (uniqueness/collision), layout unit (eviction across tree-depth),
  route smoke (downloadable CBZ, non-zero size), worker probe (window writes land in
  structured dir).
