# Tasks

## 1. Enrichment plugin path & loading

- [x] 1.1 Add registry knob `paths.enrichment` (flag `--enrichment-dir`, env `CPPISEKAI_ENRICHMENT_DIR`, ini `[paths] enrichment_dir`, default `data/enrichment`) in `src/config/registry.hpp`; committed default in `data/cppisekai.ini` only if present. Doc comment says: enrichment plugins live here and are NOT scraper plugins.
- [x] 1.2 `src/plugin/plugin_load.cpp`: scan `data/enrichment/*` as a second, separate source; tag loaded plugins with capability `enrichment` from their manifest (`enrichment_providers`); skip them on the scraper `/plugins` page registry; keep `plugins_dev` symlink behavior identical for both dirs.
- [x] 1.3 Enrichment ABI wiring: one new method `enrich(title, kind)` where kind ∈ `detail` | `covers`; validate the envelope; envelope is glaze-compatible; on failure/unknown source return `{ok:false,error}` with no store writes; reuse `http.fetch` + proxy + cookie (non-plain) from the scraper path (D1, D2).
- [x] 1.4 Enrichment plugins: `plugins_dev/lua/enrichment-mangadex/` and `plugins_dev/lua/enrichment-mangaupdates/` (dir name == enrichment id is NOT required; keep `-mangadex` suffix style of scraper dev plugins but normal manifests declare `"capabilities": "enrichment"`). Port the goIsekai reference logic (`~/tmp/goIsekai/examples/info/{mangadex,mangaupdates}/main.lua`) into our ABI (`{title, kind}` in, `{value,url}`... no — one full slice per kind, see D2). MangaDex: `?includes[]=author&artist&tag&cover_art` for detail, `/cover?manga[]=<id>` catalogue for covers. MangaUpdates: `POST /v1/series/search`, then `GET /v1/series/<id>`. No JS, no login, key-free public endpoints.
- [x] 1.5 `data/enrichment/` (committed symlink farm or dev note in README): document that dev enrichment sources live in `plugins_dev/` and are symlinked into `data/enrichment/`. Do not commit payloads or fetched caches.

## 2. Persistence

- [x] 2.1 `src/store/store_internal.hpp`: add `TitleMetaEntity{ plugin, slug, fetched (TEXT), chosen (TEXT), fetched_at (INTEGER) }` + table `title_meta` with `primary_key(plugin, slug)`. No new deps. `create_storage` extended; a `title_meta_ensure` called at boot (CREATE TABLE IF NOT EXISTS semantics of `sync_schema`).
- [x] 2.2 `src/store/store_api.cpp` (or `store_enrichment.cpp` if split): raw-connection read/write helpers `title_meta_get(plugin, slug) -> optional<TitleMetaRow>`, `title_meta_put_fetched(...)`, `title_meta_put_chosen(...)`, `title_meta_reset_cover(...)`, `title_meta_reset_detail(...)` (clear chosen keys selectively, keep `fetched`). All writes are additive overwrites; `saved_manga` and `api_cache` are never written here.
- [x] 2.3 `title_meta` JSON shape validated on read with glaze; corrupt row = warn + treat as absent (never crash). Test: insert garbage, read returns nullopt.

## 3. HTTP API

- [x] 3.1 `GET /api/enrichment/<id>/detail?plugin=&slug=&force=0|1` and `/covers?...`: loads `title_meta`; returns stored `fetched` filtered by source id when present and `force=0`; else calls the enrichment plugin; on success upserts `fetched` (JSON array, per source, `fetched_at` set); envelope `{ ok, data, cached, fetched_at }`. On plugin failure with existing `fetched`: serve `fetched` (stale, `cached:true`); with no `fetched`: 502.
- [x] 3.2 `POST /api/enrichment/chosen` `{plugin, slug, chosen}` — full-overwrite of `chosen` under same PK; unknown keys rejected (glaze exact); returns `{ok:true}`; 400 on bad shape.
- [x] 3.3 `POST /api/enrichment/reset` `{plugin, slug, mode: "cover"|"detail"|"all"}` — clears chosen cover / chosen non-tag fields / all; keeps `fetched`.
- [x] 3.4 Detail envelope merge: in `/api/detail/<plugin>/<slug>` (and the SSR context), after the scraper envelope is built, merge `title_meta.chosen`: `chosen.title ?? source name`, synopsis/artist/author/cover same pattern; `tags` = union(source tags ∪ chosen.tags_add) − chosen.tags_hidden; cover override goes to the page as `chosen_cover` URL consumed by the detail page; also expose `enrichment: { fetched_at, chosen }` for the Alpine picker (button states, "last fetched <date>").
- [x] 3.5 Routes registered in `http_api_content.cpp` (extend `/api/detail` merge there or in `http_api_title.cpp` — match the file layout seen today; keep files ≤~200 code-loc; if the merge grows, a new `http_api_enrich.cpp` is allowed per the crown-split convention).

## 4. Detail page UI

- [x] 4.1 `templates/detail.mustache` / `detail_script.mustache`: add to saved-only view — Fetch detail / Force fetch buttons + "last fetched <date>" label; Fetch covers / Force covers. Buttons disabled in-flight. Show fetched candidates grouped per source in a picker panel (title/synopsis/artist/author radio-per-field, tags checkbox-list, covers image grid).
- [x] 4.2 Cover wizard: modal overlay listing candidate covers (thumbnails via `/cache/img?url=...&cover=1`), pick → POST chosen → replace main cover immediately; Reset cover button next to cover.
- [x] 4.3 Reset detail button: clears title/synopsis/artist/author + tags_add/tags_hidden (keeps chosen.cover if any), re-renders from source.
- [x] 4.4 Merge application client-side from the `/api/detail` response (`chosen` block) — same shape SSR and Alpine hydration; no full page reload.

## 5. Tests & docs

- [ ] 5.1 `tests/test_store.cpp`: `title_meta` round-trip (put/get/overwrite chosen, reset cover, reset detail keeps fetched, corrupt JSON tolerated).
- [ ] 5.2 `tests/test_http_enrich.cpp` (new target `test_http_enrich` or extend `test_http_img`): GET detail cache-hit vs miss vs force; POST chosen validation; reset modes; verify `saved_manga`/`api_cache` untouched byte-wise after enrichment writes.
- [ ] 5.3 Update `openspec/specs/detail-enrichment/` only via the change's delta (no direct main-spec edit unless apply-sync is invoked).
- [ ] 5.4 `openspec validate detail-enrichment-fields --strict` passes; `ctest` all green; zero warnings on build.
- [ ] 5.5 Live smoke (browser): add a manga to library, fetch detail from MangaDex + MangaUpdates, pick a tag and a cover, reset cover, reset detail; repeat tap shows cached (no network); force re-fetches.

## 6. Verification gates

- [ ] 6.1 `cmake --build build -j` zero warnings; full `ctest` pass.
- [ ] 6.2 `openspec validate --change detail-enrichment-fields` errors=0.
- [ ] 6.3 okf: persist enrichment architecture facts; `okf_validate --strict --drift` errors=0; record `decisions/detail-enrichment-fields-contract`? — update `architecture/detail-page` body with D1–D10 highlights.
