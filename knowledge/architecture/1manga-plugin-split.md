---
type: Fact
title: 1manga plugin source split into purpose modules
description: "plugins_dev/lua/1manga/main.lua is now a thin dispatcher wiring small require() modules, following the Lua runtime's confined require (reads <pluginDir>/<name>.lua or init.lua, per-state cache, path traversal denied)."
tags: [1manga, plugin, lua, require, architecture]
generated: { by: agent/mcp, at: "2026-11-16T12:53:03Z" }
status: stable
---

Layout (all under plugins_dev/lua/1manga/):
- main.lua — BASE (host.plugin.param site_url), ctx {BASE, get, text}, methods table, handle() dispatch.
- http.lua — ctx.get(url): the only UA-bearing fetch wrapper (Chrome UA, 30s timeout, adds "upstream status N" message; returns (body,nil)/(nil,err)).
- text.lua — pure text helpers: decomment, unesc (host.text.unescape∘trim), strip_tags, iso_date (M-D-Y->ISO), normalize_status (lowercase canonical set incl ongoin/releasing/publishing, finished, dropped|cancelled, upcoming), normalize_tag (underscores->space, trim, lowercase), clean_title (drops "CHapter N" / "N-eng-li" scrape noise), longest_paragraph (detail synopsis <p> heuristic, 60-char floor).
- slugmap.lua — module-level shared table requiring the genres fetch's name->site-slug mapping into search (require cache is per-state, so both handlers see one table); fixes chip lookup for non-naive slugs (e.g. slice of life).
- search.lua / detail.lua / chapters.lua / pages.lua / genres.lua — one handler factory each: return function(ctx) -> function(params).
Chapter rows now carry created_at next to release_at (identical value for this source — the row renders one date).
E2E after reload: search dungeon=59, detail, chapters 64 all with created_at, pages 6, genres 52 rows (repeatable). Daemon reload required, no rebuild (plugin is dynamic).

# Related Concepts
- [Lua plugin end-to-end audit (search/detail/chapters/pages)](../findings/lua-plugin-audit-2026-11-16.md): 1manga was split into module-per-purpose files and used as the healthy reference for the plugin fleet audit.
