# Spec Delta: cache-layout

## MODIFIED Requirements

### Requirement: Plugin-addressed structured layout

The host SHALL store cover images at `data/cache/img/<ns>/cover/<sha1-16>[-<ratio>].<ext>` and
page images at `data/cache/img/<ns>/manga/<slug>/ch<NNN>/p<NNN>.<ext>` with zero-padded chapter
and page numbers, so lexicographic filename order equals reading order.

`<ns>` SHALL be the plugin **namespace**: the filesystem-safe slug of the plugin's display name,
falling back to the plugin id when the display name is absent or has no slug-able characters.
The namespace SHALL NOT be the plugin id whenever the display name yields a slug. Two plugins
whose display names slugify to the same value SHALL share one namespace directory.

Setting (or changing) a plugin's display name SHALL change its namespace, and therefore start
a fresh cache directory; cached files SHALL NOT be migrated between namespaces.

`<slug>` SHALL be a filesystem-safe lowercase manga slug; two manga whose slugs collide SHALL
be disambiguated by suffixing the numeric ending of the manga id, so paths stay unique without
a DB lookup.

Every file under `data/cache/img/` SHALL live inside a namespace directory. The img pool SHALL
NOT carry a context-free sha fallback: the stale-layer `api/<sha>.json` pool is unaffected.

Every consumer that composes an `img/` path — the page/cover writers, the extension-free hit
lookup, the cache-full probe and the CBZ export — SHALL resolve `<ns>` through the cache's
namespace resolver, so one definition of where a plugin's files live governs all of them.

#### Scenario: Display name keys the directory

- **WHEN** a plugin with id `atsu` and display name `Atsu.moe Reader` caches a page
- **THEN** the file lands under `data/cache/img/atsu-moe-reader/manga/<slug>/...`, not under
  `data/cache/img/atsu/`

#### Scenario: Id is the fallback namespace

- **WHEN** a plugin's display name is blank, or contains no slug-able characters
- **THEN** its cache directory is keyed by the plugin id

#### Scenario: Id change keeps the cache

- **WHEN** a plugin is re-installed under a different id or folder with the same display name
- **THEN** the existing cached covers and pages keep serving, because the namespace did not
  change

#### Scenario: Zero-pad ordering

- **WHEN** ch1..ch12 pages are populated
- **THEN** `ls data/cache/img/<ns>/manga/<slug>` yields `ch001..ch012` in reading order, and
  `ls ch001` yields the page files in page order

#### Scenario: Cover overwrite on refetch

- **WHEN** a manga cover is fetched a second time for the same slug
- **THEN** the same `cover/<sha1-16>-*.` file is replaced, not duplicated

### Requirement: Reader worker writes the structured path

The reader prefetch worker SHALL write page images only into
`data/cache/img/<ns>/manga/<slug>/ch<NNN>/p<NNN>.<ext>`, using the plugin/slug/chapter context
it was nudged with. A nudge without plugin, slug or chapter number SHALL be dropped: nothing is
fetched and no file is written, because the img pool has no context-free slot.

#### Scenario: Window write lands structured

- **WHEN** the worker prefetches page 2 of chapter 3 for mangadex/slug-X
- **THEN** the file appears at `data/cache/img/<ns>/manga/slug-X/ch003/p002.<ext>` and a fresh
  `GET /export/cbz?plugin=mangadex&slug=slug-X` includes it

#### Scenario: Nudge without context is dropped

- **WHEN** the reader position carries no chapter number, so the nudge has no placement context
- **THEN** the worker fetches no page and creates no file under `data/cache/img/`

## ADDED Requirements

### Requirement: No context-free image writes

`GET /cache/img` SHALL answer 404 without fetching when the request carries neither `cover=1`
nor a complete page context, and SHALL store nothing. Cache access SHALL never create a file
directly under `data/cache/img/`; that rejection replaces the old sha-pool fallback.

#### Scenario: Bare url is refused

- **WHEN** `GET /cache/img?url=<any>` arrives with no cover flag and no plugin/slug/chapter/page
- **THEN** the answer is 404 and the fetcher is not called

#### Scenario: Pool root stays free of files

- **WHEN** a session runs several context-free image requests
- **THEN** `data/cache/img/` contains only namespace directories, never a loose `<sha>.<ext>`

### Requirement: Namespace cutover on first boot after upgrade

On the first boot where a namespace resolver is configured, the host SHALL move
`data/cache/img/` aside and recreate it empty, because its directories were keyed by plugin id
and are stale by definition. The moved tree SHALL be removed by the `.legacy-` sweep on a
following boot, and a stamp file SHALL keep the cutover to a single boot.

#### Scenario: First boot after the change

- **WHEN** the host boots with a resolver configured and `data/cache/img/<id>/` directories on
  disk
- **THEN** those directories are gone from `img/`, the tree is empty, and the cache refills on
  demand

#### Scenario: Cutover happens once

- **WHEN** the host boots again after the cutover
- **THEN** the refilled `img/` tree is untouched and only the `.legacy-img` copy is swept

### Requirement: No migration of id-keyed cache directories

No id-keyed directory SHALL be migrated into a namespace directory: the cached files are
content-addressed by URL and are re-fetched on demand. The cutover SHALL NOT run when no
namespace resolver is configured, because then the plugin id *is* the namespace and the
existing tree is live.

#### Scenario: No resolver configured

- **WHEN** a cache is constructed without a namespace resolver over an existing `img/<id>/` tree
- **THEN** the tree is left intact and indexed as before
