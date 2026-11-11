---
type: Fact
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Enrichment (alt cover/alt info): single-row title_meta (fetched raw payloads + chosen overrides, no provenance), original stays in saved_manga/api_cache for free reset, dual-path buttons (cached-or-fetch + force), tags via tags_add/tags_hidden."
tags: [detail, enrichment, cover, wizard, cache, plugin]
generated: { by: agent/mcp, at: "2026-11-11T00:57:10Z" }
status: stable
---

## Enrichment architecture facts (2026-11-11 user direction)

- Enrichment = dedicated plugins, **separate path**: `./data/enrichment/<id>/*` (NOT `./data/plugins`). Same plugin machinery (Lua/sol2, http.fetch, storage.*, log.*, ApplyProxy); only the directory and capability (`enrichment`) differ. Sources first: MangaDex + MangaUpdates (official JSON APIs), more later.
- Two fetch paths, split: **fetch detail** (title, synopsis, artist, author, genre/tags) vs **fetch covers** (candidate covers only — far heavier).
- Trigger: explicit user buttons on the library detail page (saved manga only). Never implicit.
- **Original** = `saved_manga` (name/cover, captured at add-to-library) + stale-cached source-plugin detail in `api_cache`. **Never overwritten** → Reset cover/detail = revert to source for free.
- **title_meta** — single row per (plugin, slug): `fetched` (array of raw payloads per source, incl. provenance-at fetch time), `chosen` (user overrides: {title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[]}; values copied, no runtime provenance). Display = merge `chosen.<field> ?? source cache`. Reset detail = clear `chosen` non-tag/cover fields; Reset cover = clear `chosen.cover`.
- **Cache model**: `fetched` is kept **indefinitely** (no TTL, hidup selamanya); UI shows "last fetched <date>". Two buttons: **cached-or-fetch** (normal tap: serve `fetched` if present, else hit website) and **force fetch** (always hit website, refetch + overwrite `fetched`). Cover images cached on disk via `/cache/img` — same-URL taps never re-hit the website.
- **Merge semantics (user-selectable, not all-or-nothing)**:
  - Non-tag fields (title/synopsis/artist/author, cover): user **selects which fetched values appear** on the detail page, replacing the ones that exist.
  - Tags/genres: user **selects which tags to add**; tags on the detail page can also be **hidden/removed** per-title → `tags_hidden`, one list per manga.
  - Cover: 'fetch covers' opens a **wizard** to pick an alternative, choice replaces main cover.
- Mixed sources: all candidates from all enrichment plugins shown as one list; dupes allowed (user picks), ordered per-source for usability. Matching may be loose — user decides which data to use; nothing auto-applied.
- Matching: no forced auto-commit; uncertain matches presented as candidates user chooses from.
