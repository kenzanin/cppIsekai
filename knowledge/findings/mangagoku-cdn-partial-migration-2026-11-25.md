---
type: Finding
title: "mangagoku partial CDN migration: reader images split 2xstorage webp / waitst jpg"
description: mangagoku split reader images across two CDNs (2xstorage webp old / storage.waitst.com zin jpg new); pages.lua now matches both or emits silent empty envelopes that 404 the reader.
tags: [mangagoku, plugin, cdn, regex, reader, fixed]
generated: { by: agent/mcp, at: "2026-11-25T14:42:30Z" }
status: stable
---

## Symptom

`/read/mangagoku/a-harem-in-the-fantasy-world-dungeon?ch=29` → reader stuck; `/api/pages/mangagoku/a-harem-in-the-fantasy-world-dungeon/29?ch_id=...` → `{"ok":false,"error":"unknown chapter"}` for every chapter of this title, while `/api/chapters` returned 68 rows and other mangagoku titles kept working.

## Root cause

mangagoku migrated reader images to a second CDN (partial, per-title/per-age):

- old: `https://img-r{1,2}.2xstorage.com/.../<n>.webp` (solo-leveling, isekai-yurutto, harem older chapters)
- new: `https://storage.waitst.com/zin/<slug>/<chapter-slug>/<n>.jpg` (harem, newer uploads)

`plugins_dev/lua/mangagoku/pages.lua` matched only the 2xstorage pattern, so the page body yielded zero URLs → `{ok=true,data={}}` — a **silent empty** that `call_cached` then cached for ttl 1800s, and the route fell through to the generic 404 (empty `pages_error`, zero `chapter_pages` rows for the slug).

## Fix

pages.lua: second gmatch for `storage%.waitst%.com/.../%d+%.jpg`; index extractor generalized `/(%d+)%.[%w]+$` (was hardcoded `.webp`, which would have dropped every jpg). The `/N.<ext>` tail anchor is collision-safe: waitst thumbs are `<slug>.jpg` with no numeric tail, same property as 2xstorage `/thumb/`. Dedupe-by-index + sort unchanged; mixed-CDN chapters keep deterministic order.

## Verification

`POST /plugins/reload` (43), purge of the two stale-empty `api_cache` rows, then: ch29 = 26 pages ordered 0..25, ch1 = 41 pages, `/cache/img` 200 image/jxl 175 KB, `solo-leveling` ch1 (2xstorage path) still 55 pages. Live-proven, not unit-tested (plugin probes are curl-level; no Lua test harness).

## Gotcha for next time

`unknown chapter` with empty `pages_error` + zero `chapter_pages` rows for the slug = plugin pages action returned ok+empty. Check the CDN pattern before touching the route.
