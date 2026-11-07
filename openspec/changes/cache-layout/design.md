# Design: cache-layout

## Context

`cache.hpp` already has `ensure/store/peek(key, kind)` with a single in-memory LRU index
keyed by shard-relative path. Layout v1 writes to `img/<sha>` / `api/<sha>`. Reader
worker (`src/reader/worker.hpp`) is header-only and already computes chapter/page slots
from the plugin payload; it knows plugin/slug/context but currently discards it. libzippp
was reserved for CBZ export in `decisions/deps-and-plugins`; system libzip + libzippp header
will vendor for盔imilar use.

Long-term OKF decision: `architecture/image-cache-offline` — layout v2 readable per
plugin. This change implements it.

## Decisions

**D1 — one storage call site, one slug pragma, no schema.** Slug derivation lives in
`src/cache/slug.hpp` — pure arrow from manga title + manga id → `<slug>` (fs-safe,
lowercase, 64 char cap). Collisions are resolved by appending the last 8 hex chars of
the manga id. No DB table, no counter — because a title-child relation never needs a
joinable path (CBZ and reader address it whole). Slug changes when title changes; older
folder goes out by eviction, no migration logic above eviction.

**D2 — rel_path threading in Cache API.** `store/ensure/peek` gain an optional
`rel_path` (sub-path relative to the pool root: e.g.
`mangadex/slug-x/ch003/p002.jpg`). When supplied, the shard key stays the sha1 of the
URL (composite index key unchanged) AND the on-disk path becomes `root / kind /
rel_path`; when absent, behavior is identical to today (sha fallback). The LRU index
remains by *actual on-disk absolute path*, so eviction/mtime walk needs no slug
knowledge.

**D3 — eviction is a tree walk, once per boot + online.** Boot scan changes from
`root/{img,api}` to a recursive walk of `root/` accumulating `{path, bytes, mtime}`.
Cap and trim are unchanged. Since layout can arbitrate a large tree, the walk is at
most depth 4 + 1 level of files per chapter — negligible.

**D4 — purge-on-first-run after upgrade.** On clock-sensible boot, if a legacy flat
`/<root>/img` shard exists *with sha-named files* under it, rename it to
`/<root>/.legacy-img-<rand>` before the walk, then delete it. That is a 1-file move
(universally cheap), not a per-file migration. If the layout is already structured,
nothing happens. The same logic applies to `api/` (sha names) — stale api cache will
repopulate per method on the next online query. This is a deliberate rollback safety:
if anything goes wrong the legacy dir has `.legacy-` prefix preserved on disk until
the next run deletes it.

**D5 — worker placement through existing plumbing.** `POST /reader/position` already
carries plugin id and chapter id; the window computation already knows chapter/page.
The only new parameter is slug. Slug derivation is delegated to
`/details?id=... → slug` caching (the规范化 plugin detail response already includes
title). So the worker gains slug values lazily at request time, no extra plugin call.
Files the worker writes go to `<plugin>/<slug>/chNNN/pNNN.<ext>`; the sha-keyed cache
index still records the same file so ensure/peek semantics stay identical.

**D6 — CBZ route streams from the folder, no intermediate copy.** `GET
/export/cbz?plugin==..&slug=..` picks `.files` (cover + all `chNNN/pNNN.<ext>` below
`<plugin>/<slug>/`) and streams them straight to a zip writer. Each entry keeps its
relative path (`ch001/p001.img` style) so lexicographic order equals reading order;
cover goes in as `cover.<ext>`. Empty slug = empty-but-valid zip (not a 404). Uses
libzippp; a missing-history rollback path (if libzippp usage stalls) is minizip
vendored single-file.

**D7 — no slug/DB dependency for stale `api/` layer.** The stale envelope remains
sha-of-query-addressed. It is not DB or slug-bound; that is orthogonal and out of
scope.

## Known concerns for the backlog (do not implement here)

- Pinning: exporting the tree holds files while the LRU could evict mid-zip; pragmatic
  concern, acceptable because a served window is very recently used. Track in
  `references/image-cache-backlog`.
- Slug aliasing on title rename: old folder evicts naturally; no DB join needed.
- `.legacy-img-*` cleanup on the *next* boot is the only place the purge is discovered
  — it does not happen on the same boot that triggered it.
