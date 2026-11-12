---
type: Fact
title: "Atsumaru plugin (atsu.moe): Typesense search, CDN images, composite chapter ids"
description: "Atsumaru (atsu.moe) plugin: Typesense search, CDN images, composite mangaId:chapterId ids, epoch-ms createdAt, chapter-number dedupe, uppercase-id guard; chapters envelope now emits pages from pageCount (2026-11-12) so host read_ch rollup works."
tags: [atsumaru, plugin, lua, typesense, cdn, goisekai, live-verified]
generated: { by: agent/cli, at: "2026-11-12T01:03:29Z" }
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

See also `decisions/api-cache-lock-miss-regression` (the OTHER half of the original unknown-title symptom).
