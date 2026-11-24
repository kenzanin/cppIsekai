---
type: Architecture
title: "Four extension-source ports: Asmodeus, Bbato, Dynasty, MangaForFree"
description: Lua ports of four goIsekai extension-source plugins with live endpoint shapes and end-to-end verification.
tags: [plugin, lua, port, extension-source]
generated: { by: agent/mcp, at: "2026-11-24T12:28:33Z" }
status: stable
---

## 2026-11-24 four extension-source ports

- asmotoon (Asmodeus Scans, asmotoon.com): Asmotoon.kt over KeyoappV2. Search `?_ajax=search&q=&offset=` (JSON `html`), detail HTML (`p.l2-by`, `p.l2-kicker`, `h1.l2-title`), chapters `?_chapters_html=1&series_slug=` (JSON `rows_html`), pages `#strip-reader img.reader-page`.
- bbato (bato1.co.uk): Bbato.kt. Search `/filter?keyword=`, detail `/manga/<slug>`, chapters `/get-chapter-list?slug=` (XHR JSON `data[].chapter_slug/chapter_num`), pages `.pages img[data-number]` `src`.
- dynasty (dynasty-scans.com): Dynasty.kt + Dto.kt. Search `/search?q=&classes[]=` HTML; detail/chapters/pages from `/<dir>/<permalink>.json` (tags, taggings, pages[].url). Composite id `<dir>:<permalink>`.
- mangaforfree: source `src/all/mangaforfree` (MangaForFree.net); user shorthand manga4free. Madara theme, AdminAjax chapter mode. Search `?s=&post_type=wp-manga`, chapters POST `/wp-admin/admin-ajax.php action=manga_get_chapters&manga=<post id>`, reader `img.wp-manga-chapter-img` `src`. Raw chapters filtered out for the en catalogue per reference. Detail status is the second `.post-status .summary-content` block (first is Release).

## Verification (live daemon, 2026-11-24)

All five methods green for all four plugins: mangaforfree 2 search / 111 chapters / 7 pages; asmotoon 3 search / 9 chapters / 14 pages; bbato 2 search / 75 chapters / 25 pages; dynasty 20 search / 17 chapters / 57 pages. `/cache/img` 200 image/jxl for one page of mangaforfree (761 KB), asmotoon (329 KB), bbato (16 KB). Lua syntax checked, suite 21/21, build 0 warnings.

Note: dynasty's composite id contains a colon; direct `/plugin/dynasty/*` calls work, the host `/api/pages/<plugin>/<slug>/<ch>` route needs a saved row to resolve it.

## Bbato reader images are lazy-loaded (src is a placeholder)
On `/read/<slug>/<chapter>` every page image is `img[data-number]`, but only the first two carry a real `src`; the rest put an inline `data:image/svg+xml` placeholder in `src` and the real CDN URL in `data-src` (and always in `data-fallback`). A `data-number='notice'` image also rides in the same block pointing at `/assets/images/bbato.webp`. Reading `src` alone (the original port) yields 1 logo + 17 placeholders per chapter, which the reader renders as an empty/broken page — the fetch itself is fine (HTTP 200, ~57 KB), so it is not a Cloudflare gate. Fixed 2026-11-24: try `data-fallback`, then `data-src`, then `src`, take the first attribute that yields real URLs, and drop `data:` URIs plus `/assets/images/` (notice). Verified: suterare-seijo... ch1 = 19 ordered cdnb.snapvip.xyz pages, `/cache/img` 200 image/jxl; regressions 25 and 5 pages on two other titles. Titles whose `/get-chapter-list` answers `{"data":[],"total":0}` have no readable chapters upstream at all — that is the site, not the plugin.

# Related Concepts
- [goIsekai -> cppIsekai plugin porting matrix](../references/plugin-porting-matrix-goIsekai.md): four new ports from extension-source, not examples/plugins
