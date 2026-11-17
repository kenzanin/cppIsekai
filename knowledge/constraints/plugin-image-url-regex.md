---
type: Constraint
title: Plugin page-image regexes must match every image extension a site serves
description: A page-image regex pinned to one extension silently returns zero pages; en-thunderscan matched only .webp while the site serves .jpg.
tags: [plugins, lua, re2, pages, images, en-thunderscan, constraint]
generated: { by: agent/mcp, at: "2026-11-17T21:50:38Z" }
status: stable
---

## Rule
Never pin a page-image regex to a single extension. Match the full set the site (and its CDN) actually serves:

```lua
for _, m in ipairs(host.re2.find_all([[(https?:[^"]+.(webp|jpg|jpeg|png|avif))]], imgblock)) do
  local src = trim(host.re2.replace([[\\]], "", m[2]))  -- m[2] = capture 1
```

## Why
`plugins_dev/lua/en-thunderscan/main.lua` matched `(https?:[^"]+?.webp)` only. The chapter page serves `.jpg` and contains zero `webp`, so `find_all` returned nothing, `pages` answered `{ok=false, error="...no pages..."}`, and the reader showed "unknown chapter" while chapters and everything else were healthy. An in-daemon census of the real page printed `exts: {'jpg'}` -- the extension probe is what identifies this class of bug fastest.

Also remember host re2 tables are 1-based with `m[1]` = full match, so the extension-qualified group is `m[2]` (see project memory #5440).

## Sibling bug class
Same plugin, same failure shape: `methods.pages` errors on a *missing* prerequisite look identical to a bad scrape. When a reader page breaks but detail/chapters are fine, census the actual page body first, then check the regex.

# Related Concepts
- [Lua plugin end-to-end audit (search/detail/chapters/pages)](../findings/lua-plugin-audit-2026-11-16.md): The webp-only regex was found after the audit's plugin sweep; same failure shape as the regex bugs listed there.
