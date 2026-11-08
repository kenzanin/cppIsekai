# cache-layout Specification

## Purpose

Defines the on-disk cache layout for plugin-addressed structured storage, recursive LRU eviction, the legacy purge-once path, reader worker structured writes, and the CBZ export route.

## Requirements

### Requirement: Plugin-addressed structured layout
The host SHALL store cover images at `data/cache/<plugin>/covers/<slug>.<ext>` and page
images at `data/cache/<plugin>/<slug>/ch<NNN>/p<NNN>.<ext>` with zero-padded chapter and
page numbers, so lexicographic filename order equals reading order. `<slug>` SHALL be a
filesystem-safe lowercase slug; when two manga slugs collide the uploader SHALL suffix
the numeric ending of the manga id to make paths unique without a DB. The stale-layer
`api/<sha>.json` pool SHALL be unchanged; `img/<sha>.<ext>` SHALL remain as fallback for
context-free `GET /cache/img` calls.

#### Scenario: Zero-pad ordering
- **WHEN** ch1..ch12 pages are populated
- **THEN** `ls data/cache/<plugin>/<slug>` yields `ch001..ch012` in reading order, and
  `ls ch001` yields `p001.img` onward in page order

#### Scenario: Cover overwrite on refetch
- **WHEN** a manga cover is fetched a second time after a slug-same refresh
- **THEN** the same `covers/<slug>.<ext>` file is replaced, not duplicated

### Requirement: Recursive eviction over the tree
The LRU evictor SHALL walk the full `data/cache/` tree (any depth, any of the three
pools) and evict least-recently-used whole files across the whole tree, not per pool.
`cache_max_mb` SHALL remain the single cap.

#### Scenario: Eviction ignores layout depth
- **WHEN** the tree mixes `img/`, `api/` and nested `<plugin>/<slug>/chNNN/pNNN` files
- **THEN** eviction picks the least-recently-touched file across all of them

### Requirement: One-time purge on first run after upgrade
The host SHALL, once on boot after this change ships, purge the legacy flat
`img/`/`api/` sha pools from previous builds and rebuild fresh from network (or serve
404 while offline). No migration code SHALL be kept after migration completes. Stale
api cache will be re-populated on next successful online query per method; offline boot
after this change SHALL still render previously-viewed manga once the user resyncs
once.

#### Scenario: First boot after upgrade
- **WHEN** the host boots for the first time with only v1-style flat `img/` packages
- **THEN** `img/` and `api/` entries are replaced by the structured layout; no v1
  subpool dirs remain, and the boot log records the purge

### Requirement: Reader worker writes the structured path
The reader prefetch worker SHALL write page images directly into
`<plugin>/<slug>/chNNN/pNNN.<ext>` (not the sha fallback), given plugin/slug/chapter
context from the reader position payload. `GET /cache/img` SHALL continue to work with
the sha fallback so curl-only debug URLs remain valid.

#### Scenario: Window write lands structured
- **WHEN** the worker prefetches page 2 of chapter 3 for mangadex/slug-X
- **THEN** the file appears at `data/cache/mangadex/slug-X/ch003/p002.<ext>` and a
  fresh `GET /export/cbz?plugin=mangadex&slug=slug-X` includes it

### Requirement: CBZ export route
`GET /export/cbz?plugin=<id>&slug=<slug>` SHALL stream a CBZ (zip) of the cached cover
and chapter page images for that manga, entries named after the zero-padded file paths
so lexicographic order inside the archive equals reading order. Empty cache for a
requested slug SHALL produce an empty archive, not an error.

#### Scenario: Downloadable CBZ
- **WHEN** a slug whose pages were cached is exported
- **THEN** the response is a valid zip whose entries are the zero-padded image files,
  openable by any reader

#### Scenario: Unknown slug still 200
- **WHEN** `GET /export/cbz` targets a slug with no cached pages
- **THEN** the response is an empty-but-valid zip, not a 404

### Requirement: Slug uniqueness without a DB
Slug SHALL be derived from the manga title; the host SHALL disambiguate colliding slugs
by suffixing the last 8 characters of the manga id (no schema change). Slugs are
filesystem-safe lowercase, at most 64 characters.

#### Scenario: Two manga with same title
- **WHEN** two different manga resolve to slug `solo-leveling`
- **THEN** their page stores land under distinct directories distinguished by the manga
  id suffix, and both are addressable by `/export/cbz`
