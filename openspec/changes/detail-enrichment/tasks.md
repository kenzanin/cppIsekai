# Tasks

## 1. Plugin enrichment

- [x] 1.1 `plugins_dev/lua/mangadex/main.lua`: extend `canonical_manga` with `description` (`attributes.description.en`), `authors`, `artists` (relationships of type `author`/`artist`; `detail` passes `includes[]=author&includes[]=artist`). Unit-shape check via a fixture or manual load if no plugin test harness exists; else assert payload keys in the stale-serve envelope.
- [x] 1.2 Plugin `related` method: `GET /manga/<id>/recommendation` → collect candidate manga ids (other half of each `relationships` pair) → one batched `GET /manga?ids[]=...&includes[]=cover_art` (params must tolerate empty list) → return canonical manga list sorted by score desc. Return `{ ok = true, data = { items = ... } }` or `{ ok = false, error = ... }`.

## 2. API surface

- [x] 2.1 `GET /api/related/<plugin>/<slug>`: stale-serve via the existing `call_cached`/stale pattern (or the pages-api equivalent), passes plugin `related` envelope through; 404/`{ok:false,error}` on unknown plugin or empty id.

## 3. Detail page UI

- [x] 3.1 `templates/detail.mustache`: render synopsis (`description`), `Author` and `Artist` lines (comma-joined lists, hidden when absent), plugin attribution footer (plugin name + `/plugin/<id>/logo` with `@error` name-only fallback), and a Related section (cover grid linking into detail route; hidden silently when the fetch fails or yields nothing).
- [x] 3.2 Wire the related fetch (`/api/related/...`) in the detail page Alpine init; reuse the existing `encodeURIComponent` patterns.

## 4. Verification

- [ ] 4.1 `openspec validate --specs` (or `--change detail-enrichment`) green; `ctest` untouched (no new unit targets required unless cheap).
- [ ] 4.2 Live smoke (Chrome via blueprint): detail page for a known-good manga (e.g. 1389d660-b9b1-4c6a-81af-eab4dbf3f22b) renders synopsis, authors/artists, related covers, and plugin logo; a plugin without logo renders name only; no console errors.
