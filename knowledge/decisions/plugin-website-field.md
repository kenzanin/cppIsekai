---
type: Decision
title: "plugin manifest website field -> api rows + card link"
description: "Optional `website` field in plugin main.json manifest flows to /api/plugins + /api/enrichment rows and renders as an external-link glyph on plugin cards; absent means no link."
tags: [plugin, manifest, website, abi, ui]
generated: { by: agent/mcp, at: "2026-11-11T11:54:27Z" }
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
