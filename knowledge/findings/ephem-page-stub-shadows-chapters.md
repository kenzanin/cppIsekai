---
type: finding
title: "Unsaved pair: a page-list write minted a bare chapter row that then hid the whole list"
description: "chapter_pages_replace() created an ephem chapter entry (number 0, empty title/url/release_at) for an unsaved pair; chapters_list() then returned only that stub, so the second /api/pages open answered 'unknown chapter' and prev/next resolved to nothing. Fixed by skipping bare identities in the ephem listing branch; regression test added."
tags: [store, chapters, pages, ephem, bug, fixed, regression]
generated: { by: agent/mcp, at: "2026-11-27T13:19:15Z" }
status: stable
---

# Symptom

Second open of the same chapter on an unsaved title: `GET /api/pages/<p>/<slug>/1`
-> 404 `{"ok":false,"error":"unknown chapter"}`, and `/api/chapters` returned a
single row `{"id":"<slug>:chapter-1","number":0,"title":"","release_at":""}`.
First open worked (49 pages, prev/next correct).

# Cause

`Store::chapter_pages_replace` stores page urls in the ephemeral per-pair map
and, by design, mints the identity when absent:

```cpp
auto& c = ephem_create(plugin, slug).chapters[chapter_id];  // number 0, no title
```

`Store::chapters_list` is "rows first" (pages-local-first D1): once the map is
non-empty the route builds the chapter list from it and never asks the plugin.
A single bare identity therefore replaced the real 14/61 chapter list, and
`resolve_chapter_id(number)` had nothing to match.

# Fix

`chapters_list` skips entries that look like a page-list write and nothing else
(number 0 AND empty title, url and release_at); parsed zero-numbered chapters
carry a title (comik's "Volume 8 Chapter 0") and stay listed.

```cpp
if (c.number == 0 && c.title.empty() && c.url.empty() && c.release_at.empty()) continue;
```

SQLite rows are unaffected: saved pairs write only chapter_pages rows, so bare
identities exist in the ephem map alone.

# Evidence

tests/test_store.cpp "Store: a page-list write never mints a listed chapter row"
(pages-before-parse keeps the list empty and the urls reachable; parse then keeps
both rows and urls). Live: hunlight unsaved title, /api/pages twice in a row ->
49 pages both times, /api/chapters stays at 14 rows.

# Related Concepts
- [Chapter state: one flags bitfield per chapter row, chapters as metadata hub](../architecture/chapter-state-flags.md): related
- [hunlight plugin: Madara /m/ permalinks, no genre archives, one-page search](../architecture/hunlight-plugin.md): related
