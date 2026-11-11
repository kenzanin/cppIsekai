# Proposal

## Why

The detail page for a saved (in-library) manga shows exactly what the source
plugin returned at add-to-library time. Readers routinely find a better
cover, a more complete synopsis, or additional/other tags on another site,
but there is no way to fetch those alternatives, preview them, merge them
into the displayed detail, or revert if the merge is wrong. Today the only
options are "keep the source's data" or "edit the database by hand".

## What Changes

- New **enrichment plugins** under `./data/enrichment/<id>/` (MangaDex first,
  MangaUpdates second, more later). Same machinery as website plugins
  (Lua/sol2 runtime, `http.fetch`, `storage.*`, `log.*`, host-wide proxy);
  they differ only by directory and by capability (`enrichment`). They are
  NOT scraper plugins: they fetch metadata only, never chapters/pages.
  Reference: the official JSON APIs of both sources (verified in the
  goIsekai info-plugin examples, `~/tmp/goIsekai/examples/info/{mangadex,mangaupdates}`).
- Library **detail page** (`/view/manga/<plugin>/<slug>`, saved mangas only)
  gets three additions:
  1. **Fetch detail** button — normal tap: serve the cached fetch if present
     (0 network requests), otherwise fetch once; a separate **force fetch**
     button always hits the site boundary and overwrites the cached fetch.
  2. **Fetch covers** button — same cached-or-fetch / force pair, but only
     pulls candidate cover URLs (covers are far larger than metadata).
  3. A page-level UI block to **pick what to use**: fetched candidates from
     all sources shown as one list (dupe candidates allowed, ordered
     per-source, user decides — nothing is auto-applied); per-field replace
     for title/synopsis/artist/author; checkboxes for tags (add a fetched
     tag; per-tag hide/remove on the detail page); a cover **wizard** to
     choose an alternative cover; **Reset detail** and **Reset cover**
     buttons that restore the original source values.
- Host-side persistence: one new table **`title_meta`** (
  `plugin, slug, fetched JSON, chosen JSON`) keyed by `(plugin, slug)`.
  - `fetched`: array, one raw payload per source, with its fetch
    watermark. Never expires (no TTL — payload lives forever).
  - `chosen`: user overrides
    `{ title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[] }`
    (values copied, no provenance metadata needed).
  - Detail display merges `chosen.<field> ?? source-plugin cache`
    (`saved_manga` + `api_cache`), i.e. the original is never overwritten,
    so reset is trivial (clear the chosen fields).
  - `tags_hidden` is a per-manga list of tags to hide/remove on the detail
    page. Union of source + fetched tags minus hidden tags is rendered.
- Covers remain served through `/cache/img` (disk LRU, cache-first by URL).
  Repeated taps on already-seen candidate URLs hit disk, not the site; only
  unseen URLs fetch.

## Impact

- `src/store`: new `title_meta` table (additive, raw connection path like
  `api_cache`'s JSONB payload column or mapped slim columns — decided in design).
- `src/plugin`: loader gains a second directory (`data/enrichment/*`) and the
  `enrichment` capability family; same Lua runtime and host tools.
- `src/http`: new endpoints `GET /api/enrichment/<id>/...` (cached + force),
  `POST /api/enrichment/chosen` (persist user picks), `POST
  /api/enrichment/reset` (detail/cover), stale-read wiring for the merge
  (`chosen` rides the existing detail envelope, additive).
- `templates/detail.mustache`, `templates/detail_script.mustache`: fetch
  buttons, field picker, tag hide list, cover wizard, reset buttons,
  "last fetched <date>" label.
- `templates/top.mustache`/`detail`: no new nav (feature lives in detail page).
- `openspec/specs/detail-enrichment-fields/`: new delta spec.
- No dependency changes; no scraper-plugin contract changes; no TTL constants
  touched (`kApiCacheTtl` stays 24h for scraper detail).
- Out of scope: js/wasm enrichment runtimes (Lua only to start); enrichment for
  search/chapters pages; bulk fetch; auto-match committing data (user decides);
  MangaUpdates auth (public API key-free to start, config knob only if their
  rate limits force it); editing chapters; non-library mangas.

## Decisions to lock in design

- Single row per `(plugin, slug)` rather than per `(field, source)` — display
  merge is one read; provenance not needed because the user picks values.
- `fetched` is never expired (user-chosen), UI shows watermark so the user
  can force-refetch explicitly.
- Original = `saved_manga` + `api_cache` of the source plugin; we never
  overwrite either, so resets are deletes, not restores.
