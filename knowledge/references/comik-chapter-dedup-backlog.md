---
type: reference
title: "comik chapter dedupe by number + folded-id prune (done, live-verified)"
description: "DONE 2026-11-27: comik chapters.lua parses the Ch./Chapter number (never the volume digit), merges per-group duplicate rows by number (earliest release_at wins, number 0 stays per-id), sorts number desc / release_at asc; Store::chapters_upsert prunes ids folded away at an unchanged number but keeps rows beyond a truncated parse. Live: comik/4wl4n 306 -> 86 rows, no duplicate numbers, first chapter = Ch.1 (3q636ow), suite 22/22."
tags: [comik, plugin, lua, store, dedupe, done]
generated: { by: agent/mcp, at: "2026-11-27T07:13:49Z" }
status: stable
---

# comik chapter dedupe + folded-id prune — done

State 2026-11-27, on `main`, live-verified against `comik/4wl4n`.

## What was wrong

- `chapters.lua` parsed the chapter number with a bare digit run, so
  `Vol.0 Ch.11` became **0** and `Volume 1 Chapter 4.4` became **1** -- the
  volume digit. Ordering degenerated to "by volume", stable-sort ties left the
  source order.
- comik returns one row per scanlation group, so the same chapter arrived
  5-10x with different ids and nothing merged them: 306 rows for 86 chapters.
- `Store::chapters_upsert` only ever added/updated, so ids dropped by a later
  parse lingered forever.

## Fix

### `plugins_dev/lua/comik/chapters.lua`
- `chapter_number(label)`: `[Cc]h%a*%.?%s*(%d+%.?%d*)` first (covers `Ch.`,
  `Ch.001`, `Chapter`), plain digit run only as fallback.
- Merge by **number**: first row keeps the identity, later rows only lower
  `release_at` to the earliest release. `number == 0` stays per-id, one id is
  never emitted twice.
- Sort: number desc, tie-break `release_at` asc.

### `src/store/store_chapters.cpp`
- Prune is conservative on purpose: a row is deleted only when the parse
  dropped its id **and still lists its number** (a folded duplicate). Numbers
  past the parse's end survive, so a truncated plugin listing (keroscans stops
  logging "chapters truncated" and returns a short list) cannot delete real
  rows. Same rule on the ephem and the SQL path, inside the transaction.

### `tests/test_store.cpp`
- New subcase: same-number new id is pruned; a truncated re-parse keeps the
  tail. Existing flags subcase now asserts number 2 survives a parse that
  stops at 3.

## Verified

- `xmake -y` clean, `xmake test` 22/22.
- Live `POST /api/refresh/comik/4wl4n` then `GET /api/chapters/comik/4wl4n`:
  86 rows, zero duplicate numbers, 1..31 with .1/.2 subchapters, oldest row =
  `3q636ow` "Volume 1 Chapter 1", newest = `lenepxx` "Chapter 31".
- `/api/pages/comik/4wl4n/1|10|31` return pages plus correct prev/next.
- Note: folded ids (e.g. `7awnok5`, one group's Ch.10) are gone from the list;
  the Ch.10 row is `l52bnxe`. Read flags on folded ids are dropped with them.

# Related Concepts
- [comik search ignored ?page -- every UI page returned the same 48 cards](../findings/comik-search-pagination-2026-11-25.md): Same plugin, earlier fix.
