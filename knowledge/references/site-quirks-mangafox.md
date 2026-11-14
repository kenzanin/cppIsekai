---
type: Reference
title: MangaFox (fanfox.net) site quirks (wasm guest)
description: "fanfox mobile chapter list server-renders only for some titles (JS shell for others); chapter anchors carry no title attr; house-key detail envelope. Guest: plugins_dev/wasm/mangafox."
tags: [mangafox, fanfox, wasm, guest, site-quirks, plugin]
generated: { by: agent/mcp, at: "2026-11-14T03:42:56Z" }
status: stable
---

# MangaFox (fanfox.net) site quirks

Facts for the wasm guest (plugins_dev/wasm/mangafox, verified against live site 2026-11-14):

- Endpoints/regexes live in the guest header: search www.fanfox.net, chapters m.fanfox.net, pages via chapterfun/dm5imagefun unpacker.
- m.fanfox chapter anchors carry NO title attr: `<a href="//m.fanfox.net/manga/<slug>/vNN/cNNN/1.html">` - match href only, emit empty chapter title.
- Chapter list is server-rendered only for SOME titles (trigun_maximum 182 anchors, one_piece 1233); others (tenjou_tenge) return a ~10 KB JS shell with ZERO chapter anchors, and reader pages carry no `<option>` list either -> guests see empty chapters. Mobile JS endpoint not identified; revisit if one surfaces.
- Detail reply must use house keys (name/tags/authors[]/artists[]/release_at); the detail UI reads those, not title/author/genres.
- one_piece-scale chapter lists burn >2G guest instructions: host gas now 8G/call (kGasPerCall).

# Related Concepts
- Related to [WASM guest authoring pitfalls (wasm3, from mangafreak pilot)](wasm-guest-pitfalls.md)
