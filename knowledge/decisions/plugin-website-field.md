---
type: Decision
title: "plugin manifest website field -> api rows + card link"
description: "Optional `website` field in plugin main.json manifest flows to /api/plugins + /api/enrichment rows and renders as an external-link glyph on plugin cards; absent means no link."
tags: [plugin, manifest, website, abi, ui]
generated: { by: agent/mcp, at: "2026-11-18T01:44:41Z" }
status: stable
---

## Decision (2026-11-11, commit b0cb0ac)

Plugin `main.json` may declare a `website` URL; the host surfaces it end-to-end so the plugins page can link to the plugin's site.

- `Manifest.website` (optional) -> `PluginInfo.website` (`plugin_internal.hpp` / `plugin.hpp`), parsed in `plugin_load.cpp` (`m.website.value_or("")`). Absent = empty = no link.
- `Host::website(id)` accessor (thread-safe, `plugin_core.cpp`) — empty for unknown id.
- Exposed as `row["website"]` in `GET /api/plugins` rows and `GET /api/enrichment` rows (`http_api_admin.cpp`, `http_api_enrich.cpp`).
- Plugins page renders a small `↗` link next to the plugin name (both scraper cards and enrichment cards), `target="_blank" rel="noopener"`, `x-show` only when website present.
- Dev manifests filled: mangadex -> https://mangadex.org, enrichment-mangadex -> https://mangadex.org, enrichment-mangaupdates -> https://www.mangaupdates.com.

Backward compatible: old manifests without the field load unchanged. It is documentation metadata only — never used for fetching (fetch targets stay in plugin code).

## Update (2026-11-18, commit 0ff1923): `website` is now FETCH-RELEVANT — accept goIsekai's `site_url` alias

The field stopped being documentation-only: `Host::image_referer()` derives the per-site image Referer from it (manifest `image_referer` override, else `website` origin), and `image_engine` selects the fetch profile for /cache/img. goIsekai-ported manifests (~20 Lua plugins: fanfox, batcave, comico, dynastyscan, ...) carry the site as `site_url`, ours as `website` — the mismatch silently emptied the referer and 403'd every image fetch on referer/UA-checked CDNs (fmcdn/zjcdn.mangafox, komix.to).

- `plugin_load.cpp`: `website = m.website.value_or(m.site_url.value_or(""))` — either key wins, ported plugins need no edit (`Manifest.site_url` optional in `plugin_internal.hpp`).
- Symptom signature: plugin data calls fine, every `/cache/img` = 404, log `cache: img fetch failed ... (plugin=X)`; stock-curl repro 403, with `Referer: <site origin>` 200.
- CF-protected image CDNs that 403 even WITH referer need `"image_engine": "chrome116"` in the manifest (komix_to) — same key 4ucomics/weebcentral already used.
- Fanfox genre gotcha fixed in passing: its `genres` handler returned `{slug,name}` objects; the UI chip contract is plain name strings (search maps name->id internally).

# Related Concepts
- Related to [Host image fetch is plugin-blind: no Referer (js mangatown pilot finding)](../findings/js-image-referer-gap.md)
- Related to [Plugin page-image regexes must match every image extension a site serves](../constraints/plugin-image-url-regex.md)
