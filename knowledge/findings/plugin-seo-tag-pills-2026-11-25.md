---
type: Finding
title: Site SEO tag farms must not become detail tags; use JSON-LD genres
description: "manhwato detail rendered a wall of SEO keywords ('... english', '... online free', 'download ... eng') because its Tags pills block is a link farm, not genres. The site's Book JSON-LD `genre` array holds the real genres. Fixed plugins_dev/lua/manhwato/detail.lua to source tags from JSON-LD genres. Important: already-saved rows keep the old tags, so the two affected saved_manga rows had to be refreshed via POST /api/refresh/<plugin>/<slug>; only then did the browser show Action/Adventure/Drama/Fantasy/Seinen/Isekai with zero SEO text. General rule: reject site-provided 'tags' blocks that are title+keyword variants, and refresh saved rows after plugin tag fixes."
tags: [plugins, tags, seo, manhwato, saved-rows]
generated: { by: agent/mcp, at: "2026-11-25T07:16:05Z" }
status: stable
---

# Related Concepts
- Related to [Manhwato Lua plugin: SSR HTML via the daemon's Chrome bridge](../architecture/manhwato-plugin.md)
