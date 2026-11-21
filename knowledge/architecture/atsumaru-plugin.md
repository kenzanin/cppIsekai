---
type: Fact
title: "Atsumaru plugin (atsu.moe): Typesense search, CDN images, composite chapter ids"
description: "Atsumaru (atsu.moe) plugin: Typesense search, CDN images, composite mangaId:chapterId ids, epoch-ms createdAt, chapter-number dedupe, uppercase-id guard; chapters envelope now emits pages from pageCount (2026-11-12) so host read_ch rollup works."
tags: [atsumaru, plugin, lua, typesense, cdn, goisekai, novel, live-verified]
generated: { by: agent/mcp, at: "2026-11-21T10:31:51Z" }
status: stable
---

## Atsumaru plugin (atsu.moe), shipped 2026-11-11 (638e4bf, + 4091b5c, 306230f)

- Folder `plugins_dev/lua/atsumaru/` (main.json + main.lua + logo.png), symlinked into `data/plugins/atsumaru`. ABI envelope `{ok, data|error}` mangadex-style. Capabilities: search, detail, chapters.
- Ported from goIsekai reference corpus (`references/goisekai-plugin-examples`), adapted: `host.http.get_body` -> `host.http.fetch` envelope, `host.text.url_encode` -> `host.url_encode`, table-return `{ok,data}`.
- Endpoints: search = Typesense `GET /collections/manga/documents/search?q=&filter_by=&query_by=title,englishTitle,otherNames,authors&page=&per_page=20`; detail = `/api/manga/page?id=` (mangaPage: title, synopsis, status, authors[], genres[], poster{image|id}); chapters = `/api/manga/allChapters?mangaId=`; pages = `/api/read/chapter?mangaId=&chapterId=` (readChapter.pages[].image).
- **Images are NOT served by the origin**: `atsu.moe/static/...` returns 410 Gone. Relative paths resolve against `https://cdn.atsu.moe/static/` (three shapes: `/static/x.jpg`, `x.jpg`, absolute). Poster may be a table (`poster.image` or `poster.id`).
- **Composite chapter ids**: `mangaId:chapterId` (`H6P2Z:PSwqrE56`); pages parses `^([^:]+):(.+)$` and needs both ids for `/api/read/chapter`. Chapter number: `tonumber(c.number)` with label-fallback.
- **Duplicate chapter numbers (306230f)**: the site ships second releases of the same number (e.g. two `Chapter 46`, ids `htvhu`/`Fxwzpz`, one is likely a minor-edit re-release). The host routes pages/CBZ by chapter NUMBER, so a duplicate number is unreachable and only duplicated the list. Plugin now dedupes by number (first wins) before sorting ascending; live-verified 48 unique numbers, pages route OK.
- **createdAt is epoch milliseconds** (int64) -> /1000 for unix seconds (goIsekai knowledge `atsumaru-released-at-epoch-millis`).
- Genres: 16 static slugs from `/api/explore/availableFilters` (adult entries excluded; filter `genreIds:=[slug,slug]`).
- **Uppercase ids (4091b5c)**: atsumaru ids (`H6P2Z`) carry uppercase; the host `valid_title_pair` guard was lowercase-only and 404'd every atsumaru detail with `unknown title`. Guard is now case-preserving; unit-locked in test_http_enrich.
- **Logo**: MangaUpdates-style mascot kept for enrichment-mangaupdates (`logo.png` from mangaupdates.com/images/mascot.png, 55KB); atsumaru logo.png ported from the goIsekai example.
- Live-verified (2026-11-11): search 'one piece' 20/43, detail H6P2Z OK after guard fix, 48 chapters (deduped), 31 pages (ch1), 16 genres; `/plugins/reload` count=4.

## Novel vs manga: use `medium`, never `hasUploads` (2026-11-21)

- `mangaPage.hasUploads` is `false` for BOTH real manga and novels. First detection used it and falsely flagged `5QVaH` (Comic, 31 real page images) as a novel. The HAR capture `tmp/atsumaru.har` has no `hasUploads` key at all but does carry `medium`.
- Discriminator: `medium == "Novel"` (case-insensitive substring); fallback `type` containing "novel". `type` alone is a format label (`Manga`/`Manwha`) and cannot identify novels (`Owlv` is `Manwha` + `medium: Novel`).
- Plugin emits `is_novel` + `url` in the detail envelope. Because row-served details come from `saved_manga` (which has no column for these), `materialize_detail` stores them in generic `plugin_state` under `host/novel/<plugin>/<slug>` and `host/detail_url/<plugin>/<slug>`, and `saved_detail_response` restores them. No schema change.
- Detail UI: amber `novel · no images` badge, warning with link to the original site, and the Start/Continue button hidden (computed `display:none`, verified in real Chrome).
- Real novel (`Owlv`): all 1174 chapters `pageCount: 0`, `/api/read/chapter` returns `pages: []`, site redirects to `/novel/...`. Host now propagates the plugin's real error (`atsumaru: no pages`) instead of the misleading `unknown chapter` 404.
- `medium: Novel` also covers `DmcL` (type `Manga`, `hasUploads: true`, 693 chapters) — proof that neither `type` nor `hasUploads` can be trusted. Because the flag is materialized into `plugin_state` at detail fetch, an existing saved row keeps the OLD value until `POST /api/refresh/<plugin>/<slug>`; a plugin-logic change alone does not retroactively fix cached rows.

See also `decisions/api-cache-lock-miss-regression` (the OTHER half of the original unknown-title symptom).
