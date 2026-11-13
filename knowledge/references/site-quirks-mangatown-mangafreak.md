---
type: Reference
title: "Site quirks: mangatown + mangafreak (porting notes)"
description: "Per-site endpoint/markup facts learned while porting: mirrors, param names, date and genre markup, cache behavior."
tags: [mangatown, mangafreak, site-quirks, plugin, porting]
generated: { by: agent/mcp, at: "2026-11-13T15:04:43Z" }
status: stable
---

# Site quirks: mangatown + mangafreak

## mangatown (runtime: js)
- Hosts: `www.mangatown.com` desktop, `m.mangatown.com` mobile detail/chapters, `sso.mangatown.com` mirror (HAR capture used sso; both serve same paths).
- Search: desktop `/search.php?title=<q>` two-pass scrape (numeric-slug skip); house param is `title`/`q` — the JS guest now matches.
- Chapters live on the MOBILE detail page; each li ends `<span class="time">Jul 22,2014</span>` -> parse `Mon D,YYYY` manually to epoch (quickjs Date.parse unreliable for this form); missing time = 0.
- Pages: desktop `chapterfun.ashx` returns 2 images/request w/ Dean-Edwards packed JS; mobile per-page `<img id="image">` fallback.
- Genres: advsearch checkboxes `name="genres[Name]"` — 34 names (4 Koma...Youkai); guest `genres` returns the static name list; genre-filtered search would POST `genres[Name]=on` (not implemented).
- Detail genres: `Genre(s):<a href="/directory/0-<slug>-0-0-0-0/" title="Name">Name` blocks.

## mangafreak (runtime: wasm)
- DNS: `mangafreak.net` does not resolve; serve from `https://ww3.mangafreak.me` (manifest website + BASE).
- Search page `/Find/<query>`: results are `class="manga_search_item"` blocks; `manga_title` exists ONLY on detail pages — one pattern set cannot serve both.
- Chapter list = series table; ~1200 chapters -> match JSON ~1.3 MiB; per-chapter anchors `/Read1_<SLUG>_<N>`.
- Pages: guest needs id form `SLUG:Read1_SLUE_CH` (chapters data id) -> `/Read1_...`; wrong shape 404s and burns gas.
- Responses cache per full query: ALWAYS bump `v=` when testing or a stale payload masquerades as a fresh failure; upstream slow spells return empty result sets that get cached (self-heals on next distinct v).

## Cross-site
- Plugin endpoint responses are cached by the host: bump `v` for every test.
- House search envelope `{items,total}`; UI breaks on bare arrays.

# Related Concepts
- Related to [goIsekai -> cppIsekai plugin porting matrix](plugin-porting-matrix-goIsekai.md)
- Related to [WASM guest authoring pitfalls (wasm3, from mangafreak pilot)](wasm-guest-pitfalls.md)
