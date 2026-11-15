---
type: finding
title: "Detail vs library title mismatch: fixed (glaze map insert invalidated the title view)"
description: "Detail h1 was empty ('…') while the library card showed a title. Root cause was NOT the api_cache jsonb/ok theory recorded on 2026-11-15: json_type(payload,'$.ok') is 'true' and json(payload) emits literal true, so envelope_ok always passed. The real bug was inside apply_chosen's ensure_name() (src/http/enrich_internal.hpp): 'd[\"name\"].data = *s' assigned through a reference into the SAME glaze object, and glaze's object_t is a flat map (ordered_small_map) whose insertion of \"name\" relocates the title node -- the assignment then read moved memory and wrote an empty name (live /api/detail showed \"name\":\"\"). Fixed by copying the title into a local std::string first; regression test 'apply_chosen derives detail name from the scraper title' added. Committed 9966ecb."
tags: [detail, apply_chosen, glaze, generic, ordered_small_map, dangling-reference, name, title, mangafreak, fixed]
generated: { by: agent/mcp, at: "2026-11-15T02:42:38Z" }
status: stable
---

## Symptom
Detail page (/view/manga/<plugin>/<slug>) h1 rendered `…` for mangafreak titles while the library card of the same manga showed a title.

## Root cause (falsifies the earlier jsonb theory)
- Cached envelopes are fine: `json_type(payload,'$.ok')` = `true`, `json(payload)` emits literal `true`, so `envelope_ok()` passed all along. There is no int->bool coercion bug and no stale-envelope consumer is broken by it.
- The bug lived in `apply_chosen()` -> `ensure_name()` (src/http/enrich_internal.hpp). Scraper envelopes carry `title` only while the detail h1 reads `name`, so ensure_name copied title -> name with `d["name"].data = *s`, where `*s` is a reference into the title node of the SAME `glz::generic` object. glaze's `object_t` is a flat map (`ordered_small_map`), so inserting the new `name` key relocates the title node; the assignment then read moved memory and produced an empty string. Live `/api/detail` showed `"name":""` next to a non-empty `title` -- the exact signature.

## Fix (committed 9966ecb)
- src/http/enrich_internal.hpp: copy the title into a local `std::string` before inserting `name`.
- tests/test_http_enrich.cpp: regression case `apply_chosen derives detail name from the scraper title` (title-only envelope, slug not in saved_manga).
- Rule of thumb: inside a glaze object, never assign a node from a reference into that same object; copy first.

## Verification
- Live: `/api/detail/mangafreak/Dungeon_Tou_De_Yadoya_O_Yarou_...` now returns `name` == the scraper title.
- test_http_enrich 7/7 (51 assertions); full `xmake test` 18/18 targets green; goldens re-locked without the update flag.

## Side observations (not fixed, not blocking)
- 343 of 349 api_cache rows have a NULL payload (rows written before the payload column existed / failed jsonb writes). `api_cache_get` already treats an empty payload as a row-shaped miss, so these re-fetch quietly; a cleanup would only save lookups.
- The 'chapter select is empty' report never reproduced: ch 63..1 renders on /read and ch 063..044 on detail. Stale browser shell, addressed by `Cache-Control: no-cache` on html() (commit c09cb7d).
- Separate bug fixed the same day: [Start reading] opened the LAST chapter because `startOrContinue()` took `chapters[0]` from a newest-first list; it now takes the last row (= lowest chapter number).
