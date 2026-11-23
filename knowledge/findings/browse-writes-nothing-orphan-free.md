---
type: Finding
title: "Routing unsaved pairs to memory removes the orphan class entirely (measured 73%)"
description: "Measured before the fix: 24,862 of 33,933 chapters rows (73%) belonged to 250 never-saved pairs. After library-ownership: unsaved pairs write no SQLite rows at all (verified live: rows_while_browsing=0, flags visible via /chapter/state from RAM, 0 after restart), and plugin removal keeps library rows as derived orphans. So orphan cleanup is unnecessary for browse traffic - only legacy rows need the purge route."
tags: [store, orphan, ephemeral, measurement, fixed, library]
generated: { by: agent/mcp, at: "2026-11-23T10:09:11Z" }
status: stable
---

## Measurement (before)

```
saved_manga            121 pairs
chapters            33,933 rows   -> 24,862 rows (73%) under 250 unsaved pairs
chapter_pages        8,866 rows   ->  2,517 orphan
reader_positions        92 rows   ->     26 orphan
plugin_state           380 rows   ->    350 were per-pair host keys
```

## After (live verification, 2026-11-23)

- Open + read an unsaved title: `SELECT COUNT(*) FROM chapters WHERE pair` stays **0**;
  `/chapter/state` reports `done:true` from the in-process map.
- Restart the daemon: rows still 0, state `null`, the 121 saved rows untouched.
- Save the browsed title: 25 chapter rows appear, `flags&1` preserved, `read_ch = 2 / total 25`.
- Move a plugin folder aside: 13 entries flip to `state=orphan`, refresh answers
  `plugin removed`, `purge-orphans` counts 0 (nothing to purge); restoring the folder makes
  them `live` again with no writes.

## Rule

Do not add GC for browse dust: it cannot exist anymore. The only cleanup left is the explicit
`POST /api/library/purge-orphans` for library entries whose plugin the user removed for good, plus
the existing `cache_prune_expired` for `api_cache`.

# Related Concepts
- [Library ownership: RAM for browse, SQLite for saved, plugin memory derived as orphan (IMPLEMENTED)](../decisions/library-ownership-model.md): Evidence for the implemented model: 73% of chapter rows were browse dust before the routing change.
