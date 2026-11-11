# Design

## Context

See proposal.md. Constraints from the existing codebase:

- Catalog/library write path: `saved_manga(plugin, slug, name, cover, added_at)`
  is written once at add-to-library time (`Store::save_manga`); nothing else writes it.
- Scraper detail rides `api_cache` (24h TTL) via `call_cached` in `src/http/stale.hpp`;
  the detail page hydrates via `/api/detail/<plugin>/<slug>` (+ chapters) which returns
  the merged envelope.
- Plugin loading scans `data/plugins` for website plugins (registry in
  `src/plugin/plugin_load.cpp`), with `plugins_dev/<lang>/<site>` symlinked in for dev.
- Plugin ABI is JSON-text data-in/-out with a single wrap point (`call_cached`);
  host tools: `http.fetch`, `storage.*`, `log.*`.
- `title_meta` does not exist yet (only mentioned in `normalization-contract`).
- The goIsekai reference implementation (`~/tmp/goIsekai/examples/info/{mangadex,mangaupdates}`)
  demonstrates the shapes: one ABI call (`getEnrichment`), kinds per feature, JSON `{value,url}` items,
  memoized VM per title. We deliberately differ: our kinds collapse into ONE call returning
  the full slice (title/synopsis/authors/artists/tags/covers), because detail and covers are
  two different buttons on the page, not per-kind round trips.
- MangaDex official API: search/manga(`?includes[]=author&artist&tag&cover_art`),
  `/cover?manga[]=<id>` catalogue, recommendations via `/manga/<id>/recommendation`.
- MangaUpdates official API: `POST /v1/series/search {search}`, then
  `GET /v1/series/<id>` — full record carries title/associated/description/genres/authors/image.url.original.

## Goals / Non-Goals

**Goals:**
- Library detail page: explicit Fetch detail / Fetch covers buttons per saved manga
- All enrichment candidates shown at once (mixed sources), user picks; nothing auto-applied
- Per-field replace for title/synopsis/artist/author; additive + hide per tag/genre
- Cover pick wizard; Reset cover / Reset detail revert to the source
- Zero network requests on repeated taps (covers via disk cache, metadata via `fetched`)
- No schema migration risk for existing data: `title_meta` is additive

**Non-Goals:**
- Enrichment on search/chapter list/reader pages
- Auto-committing matches (no "apply best match" checkbox — user always picks)
- TTL on enrichment (`fetched` lives forever by user decision)
- js/wasm enrichment runtimes in this change (Lua only)
- Editing chapters, deletion of `fetched`, provenance metadata per chosen field
- Non-library mangas, bulk refresh, background prefetch
- MangaUpdates API-key flow unless forced by rate limits (config knob deferred)

## Decisions

- **D1 — Separate `data/enrichment/<id>/` directory, same plugin machinery.**
  The host scans both directories with the same Lua loader and host tools; enrichment
  plugins declare capability `enrichment` and never dir-land as scrapers. Why: one loader,
  one tool surface, one proxy path, one runtime; the only new thing is a second scan root
  and a capability filter for which directory feeds the `/plugins` page. Alternative:
  a second plugin runtime purely for enrichment — rejected (duplicated code path, no win).

- **D2 — One ABI method `enrich(title)` returning a full metadata slice, not per-kind calls.**
  MangaDex reference does kind-by-kind with a memoized VM; we simplify: the call fetches
  everything needed for Fetch-detail or Fetch-covers in one trip (detail: 1 request
  search+includes + ~0; covers: one `/cover?manga[]=` catalogue request). Two call shapes:
  `enrich(title, "detail")` vs `enrich(title, "covers")`. Why: the page has exactly two
  buttons; per-kind round trip adds UI complexity and re-fetch logic we don't need.
  Envelope: `{ ok: true, data: { title?, romaji?, synopsis?, authors[], artists[], tags[], covers[] } }`.
  `covers[]` entries carry the full URL (bypassing a `fileName`-only contract) so `/cache/img`
  can key on the exact URL the user saw. Alternative considered: kind-per-call like goIsekai —
  rejected (more wire shapes to test, no concrete gain).

- **D3 — Single row per (plugin, slug) in `title_meta`: `fetched` + `chosen`, no provenance.**
  `fetched`: JSON array `[{ source, fetched_at, data { ... } }]` — everything the
  enrichment plugin(s) returned, raw. `chosen`: JSON object
  `{ title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[] }`
  holding user-copied values. Display merge:
  `chosen.title ?? api_cache detail (source plugin) .title`, etc. Why:
  - one read for merge, cheap; no normalization-level per-field provenance to maintain;
  - Reset = `DELETE FROM title_meta WHERE plugin=? AND slug=?`, or clear `chosen` only
    (keep `fetched` so the wizard/re-merge after reset costs zero requests);
  - source plugin data is NEVER overwritten — `saved_manga` holds only name/cover from
    add-to-library; `api_cache` holds the source detail payload untouched.
  Alternatives: display table populated from JSON (rejected: two sources of truth,
  sync bugs); per-field `title_meta_field` rows (rejected: read amplification for one
  page view).

- **D4 — `chosen` save is an HTTP POST carrying the whole `chosen` object, host merges under PK.**
  `POST /api/enrichment/chosen {plugin, slug, chosen: {...}}` — body validated by glaze,
  written wholesale; unknown keys rejected; `tags_hidden` is a plain string array.
  Why: single-writer (the page), no partial-update semantics to define.

- **D5 — Cover images flow through `/cache/img` with the chosen URL as the key.**
  Cover wizard lists URLs; on choice the host stores the chosen URL in `chosen.cover`;
  renderer passes the URL to the existing `/cache/img` route, which already does disk-LRU +
  `image_format` transcode + cover_max_px cap. Why: repeated taps reuse disk; no new
  cache code. Mangadex URLs on `uploads.mangadex.org` need `plain=true` handling — the
  image cache already serves these today (jxl transcode), so nothing new.

- **D6 — Cover candidates forMangaDex: `/cover?manga[]=<id>` catalogue, locale sort (en, ja, rest), 256/512/full.**
  The manga payload carries only the primary `cover_art`; the catalogue lists every
  edition. MangaUpdates: `image.url.original` (single image per series). Both plugins
  return `covers[]` as flat URLs; the page renders thumbnails (256px) via `/cache/img`
  with `cover=1`. Why: matches the reference implementation's working id→cover path
  and our existing thumbnail route.

- **D7 — No TTL on enrichment; force button bypasses stored `fetched`.**
  `fetched` has no expiry (user decision: cache hidup selamanya + force overrides).
  Normal tap checks `fetched` presence → returns it; force tap always calls.
  UI shows "last fetched <date>" from `fetched[].fetched_at`. Why: explicit refresh is
  strictly user-controlled; no silent background refetch burning upstream quota.

- **D8 — Tag model: `tags_add[]` (fetched values copied in) + `tags_hidden[]`.**
  Union = `(source tags ∪ chosen tags_add) − chosen tags_hidden`. Hiding is per-manga,
  persisted in `chosen`. Why: one place to keep it, no cross-ref to the source payload.

- **D9 — `title_meta` uses the existing raw-connection pattern (like `api_cache`'s JSONB column).**
  sqlite_orm has no JSONB type; keep two TEXT columns `fetched`/`chosen`, store through
  the raw connection (same precedent as `ApiCacheEntity`'s payload). Why: no glaze row
  codegen, no schema adapter wedge; additive table, one PK, no FKs.

- **D10 — Enrichment matches are user-resolved; no confidence gating.**
  Plugins return candidates; MangaDex/MangaUpdates hits are shown as fetched data —
  the user picks. If a series matched wrongly, the user just doesn't apply it or hits
  force/another source. Cold path: no match → empty `data` → page shows "no candidates
  found" and the buttons stay available.

## Risks / Trade-offs

- [Spam-tapping force fetch hammers the upstream API] → force is user-triggered only;
  MangaDex rate-limits are tolerated for a personal reader; a debounce on the button is
  applied client-side (disable while pending); no automatic retries.
- [Stale `fetched` forever] → `"last fetched <date>"` label always visible; force button
  adjacent. User accepted; docs must say it explicitly.
- [MangaDex/MangaUpdates API shape drift] → both plugins isolate upstream parsing in
  one function; if the envelope comes back empty the UI degrades to "no candidates", never
  crashes.
- [Cover URL rot (file renamed at source)] → chosen ones 404 in `/cache/img`; UI shows
  broken-image fallback (already have `onerror` pattern); reset cover restores the source URL.
- [`title_meta` growth] → one row per saved manga, ~tens of KB worst case (raw payloads);
  fine for a personal library.
- [Tag vocabulary mixing] → tags are compared case-insensitively for `tags_hidden` removal;
  UI shows raw spellings.
- [Reset detail leaves `chosen.tags_add` when user wanted "remove tag I added"] → reset
  detail clears tags_add + tags_hidden entirely; per-tag removal remains available anytime
  (hide individual tag), full reset is the broader hammer.
