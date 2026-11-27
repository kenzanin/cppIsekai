---
type: reference
title: "hunlight plugin: Madara /m/ permalinks, no genre archives, one-page search"
description: "hunlightcomics.com runs WordPress Madara with /m/<slug>/ permalinks and POST /m/<slug>/ajax/chapters/; no genre archives (404) and search ignores ?page, so the plugin has no genres method, search is one page, and browse pages through /manga/page/N/."
tags: [plugin, hunlight, madara, wordpress, scrape, chapters, search]
generated: { by: agent/mcp, at: "2026-11-27T13:19:15Z" }
status: stable
---

# hunlight plugin (plugins_dev/lua/hunlight)

Single-file Lua plugin (main.lua + main.json, no logo: the UI falls back to the
initial when /plugin/hunlight/logo 404s). Source material: tmp/hunlight.har.

## Site shape (verified live 2026-11-27)
- Madara theme, permalinks rewritten to `/m/<slug>/` (stock `/manga/` is only
  the archive index).
- Search: `GET /?s=<q>&post_type=wp-manga` renders `row c-tabs-item__content`
  cards (cover anchor first, then post-title h3). `?page=N`/`paged=N` return
  the same 10 cards, and the page prints no pager, so the result set is one
  page: plugin reports total=#items, pages=1.
- Browse: `GET /manga/` + `/manga/page/N/` (10 cards, `page-item-detail`
  blocks). No pager numbers, only an "Older Posts" link to page N+1 -- the
  plugin reports pages=page+1 when that link is present, otherwise page.
- No genre archives: `/manga-genre/<g>/` is 404 and the title page carries no
  genre markup, so the plugin omits the `genres` method entirely (search_script
  tolerates a failed /plugin/<id>/genres fetch).
- Title page: og:image cover, `description-summary` synopsis, Status block
  ("OnGoing" -> host.text.normalize_status -> "Ongoing"), authors/artists via
  `manga-author/` + `manga-artist/` rel=tag links.
- Chapters: `POST /m/<slug>/ajax/chapters/` with `X-Requested-With:
  XMLHttpRequest`; each `li.wp-manga-chapter` has the link text "Chapter N" and
  a `<span class="chapter-release-date"><i>August 27, 2026</i>` date, parsed
  with host.text.date_epoch ("%B %e, %Y" layout).
- Pages: `img.wp-manga-chapter-img`, url in `src` (padded with tabs/newlines --
  trim) with data-src as the fallback; images live under
  `/wp-content/uploads/WP-manga/data/<manga>/<chapter>/NN.jpg|png` and need the
  plugin website as referer (image-referer-domain-match does this for free).

## Chapter ids
Composite `<slug>:<chapter-slug>` (e.g. `...:chapter-1`), because the pages
method only receives `{id}` and must rebuild `/m/<slug>/<chapter-slug>/`.
Parsed dates land as unix seconds, numbers ascend (source lists newest first).

## Gotchas
- The axios POSTs to `/wp-admin/admin-ajax.php` in the HAR are view counters
  (`{"success":true,"data":"ok"}`), not needed.
- Plain-Lua block slicing (find marker, cut to next marker) is used instead of
  lazy regex over 60-100 KB pages; the card shapes differ between search and
  archive, so harvest() takes the marker.
- Offline parser harness: `lua tmp/check_hunlight.lua` replays the HAR bodies
  against stubbed host tools (tmp/ is gitignored).

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](plugin-system.md): related
- [Plugin chapter numbers are untrusted: order, duplicates and placeholder 0](../constraints/plugin-chapter-number-placeholders.md): related
