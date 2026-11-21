---
type: Decision
title: "Image referer follows the URL's own site domain"
description: "Host picks the image Referer by matching the image URL host against registered plugins' website/referer domains (subdomains count), with the title plugin as fallback."
tags: [image, referer, enrichment, host]
generated: { by: agent/mcp, at: "2026-11-21T08:24:50Z" }
status: stable
---

# Decision (2026-11-21)

MangaDex serves a placeholder image ("You can read this at: MANGADEX") when an image request carries a foreign Referer. Enrichment covers were rendered with the title plugin's id (e.g. `1manga`), so `Host::image_referer` produced `https://1manga.co/` and every alternative cover became that placeholder.

## Fix
`Host::image_referer_for_url(url, fallback_id)` in `src/plugin/plugin_core.cpp` scans registered plugins for one whose website or image_referer shares the image URL's site (exact or subdomain), and uses that plugin's referer. Falls back to the title plugin. Both curl paths in `src/http/http.cpp` use it.

Data-driven, no site hardcoding: CDNs on unrelated domains still inherit the title plugin's referer (komacdn, mghcdn, mfcdn keep their existing behavior).

## Also
- `plugins_dev/lua/enrichment-mangaupdates/main.lua`: related reading merges `recommendations` (weighted) + `related_series` (spin-off/side-story), de-duplicated by series id; integral ids formatted with `%d` (no `.0`); cap removed.
- `plugins_dev/lua/enrichment-mangadex/main.lua`: cover/recommendation caps removed earlier.

## Verified
Direct referer matrix (none/mangadex/1manga) proved hotlink protection. After purge + restart, `/cache/img?...&plugin=1manga` served a 562x800 real cover; browser hero img naturalWidth 562. MU for Dungeon Meshi returned 7 items (5 + 2 related_series). Suite 20/20.

# Related Concepts
- [host.text.clean: lexbor entity fixpoint + RE2/utf8proc numeric sweep](host-text-entity-fixpoint.md): Both keep host-side cross-runtime behavior data-driven; image referer now follows the URL domain instead of the caller plugin.
