---
type: Fact
title: "Atsumaru plugin (atsu.moe): Typesense search, CDN images, composite chapter ids"
description: "Atsumaru (atsu.moe) scraper plugin: Typesense search, JSON detail/chapters/pages, composite mangaId:chapterId ids, images via cdn.atsu.moe (origin 410), createdAt epoch-ms; ported from goIsekai corpus."
tags: [atsumaru, plugin, lua, typesense, cdn, goisekai, live-verified]
generated: { by: agent/mcp, at: "2026-11-11T11:55:30Z" }
status: stable
---

## Atsumaru plugin (atsu.moe), shipped 2026-11-11 commit 638e4bf

- Folder `plugins_dev/lua/atsumaru/` (main.json + main.lua + logo.png), symlinked into `data/plugins/atsumaru`. ABI envelope `{ok, data|error}` mangadex-style. Capabilities: search, detail, chapters.
- Ported from goIsekai reference corpus (`references/goisekai-plugin-examples`, `examples/plugins/lua/atsumaru`), adapted to cppIsekai ABI: `host.http.get_body` -> `host.http.fetch` envelope, `host.text.url_encode` -> `host.url_encode`, table-return `{ok,data}`.
- Endpoints: search = Typesense `GET /collections/manga/documents/search?q=&filter_by=&query_by=title,englishTitle,otherNames,authors&page=&per_page=20`; detail = `/api/manga/page?id=` (mangaPage: title, synopsis, status, authors[], genres[], poster{image|id}); chapters = `/api/manga/allChapters?mangaId=`; pages = `/api/read/chapter?mangaId=&chapterId=` (readChapter.pages[].image).
- **Images are NOT served by the origin**: `atsu.moe/static/...` returns 410 Gone. Relative paths come in three shapes (`/static/posters/x.jpg`, `posters/x.jpg`, absolute); relative ones resolve against `https://cdn.atsu.moe/static/` (strip leading `/static/` then prefix CDN). Poster may be a table (`poster.image` or `poster.id`).
- **Composite chapter ids**: `get_page_list` needs both ids, so chapters return `id = mangaId .. ':' .. chapterId`; pages parses `^([^:]+):(.+)$`. Chapter number: `tonumber(c.number)` with label-fallback; site returns ascending, plugin sorts ascending (deterministic, same as mangadex `order[chapter]=asc`).
- **createdAt is epoch milliseconds** (int64) — divide by 1000 for unix seconds (see goIsekai knowledge `atsumaru-released-at-epoch-millis`).
- Genres: 16 static slugs from the site's `/api/explore/availableFilters` (adult entries deliberately excluded; search filter `genreIds:=[slug,slug]`).
- Live-verified (2026-11-11): search 'one piece' 20/43 hits, detail OK, 1498 chapters, 54 pages (cdn.atsu.moe URLs), 16 genres; `/plugins/reload` count=4.

## Known gap
Detail route `/view/manga/atsumaru/<id>` intermittently shows unknown-title because of `decisions/api-cache-lock-miss-regression` (db lock treated as cache miss) — not an atsumaru bug.

# Related Concepts
- [goIsekai reference corpus: example plugins + HAR captures](../references/goisekai-plugin-examples.md): Atsumaru was ported from the goIsekai reference corpus (example plugin + HAR)
