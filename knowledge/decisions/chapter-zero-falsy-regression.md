---
type: Decision
title: "reader: chapter 0 is valid — falsy-zero regression fixed + unit-locked"
description: "Chapter 0 (prequel) and fractional chapters dropped by truthiness/range checks on 0 — reader+position fixed 2026-11-10, cache-placement guards fixed 2026-11-18 (2f398e3)."
tags: [reader, ch0, falsy, pages-api, test_pages, cache, img-route]
generated: { by: agent/mcp, at: "2026-11-18T13:58:35Z" }
status: stable
---

Symptom: /read?ch=0 opened chapter 1 or the stored resume chapter, never chapter 0 (manga mangadex 1389d660 has ch 0). Root causes: (1) reader.mustache boot used Number('{{resume_ch}}') || 1 (0 -> 1) and `if (qCh)` (0 discarded); (2) src/http/http_pages.cpp clamped resume ch with std::max(1, ...) (0 -> 1); (3) /reader/position used chapter_number > 0 / <= 0 (0 meant absent). Fix: Math.max(0, ...) + Number.isFinite gates client-side; std::max(0, ...) server-side; /reader/position chapter-number resolution extracted to pure position_api.hpp resolve_chapter_number (std::optional<double> incoming: absent keeps stored, explicit 0 wins). Route /api/pages moved from <int> to <double> in 3304fe2 for fractional chapters; fmt_ch emits integral 2 (not 2.000000) and 1.5 untrimmed. Unit locks: tests/test_pages.cpp ch 0 + fractional 1.5 (383c257); tests/test_position.cpp resolve_chapter_number explicit-0 vs absent (43d53c2) — ctest 14/14. Related: architecture/reader-readahead, architecture/reader-page-progress.

## 2026-11-18 recurrence: cache-placement layer (2f398e3)

Same class, third layer: comico prologue ch=0 rendered a blank reader because the
cache placement guards used `chapter < 1` / `<= 0` — the img never got a placement
context ("cache: img no placement context, refusing", 404) even though /api/pages
resolved ch 0 fine. Three more sites fixed: `Cache::img_store` + `Cache::img_ensure`
guards (src/cache/cache_img.cpp) -> `chapter < 0`; /cache/img route pctx gate
(src/http/http_api_content.cpp) -> `ch >= 0`; `chapter_cached` full-chapter check
-> `< 0` (ch0 dir ch000-<id8> is a real name). Rule of thumb now proven three
layers deep: EVERY new numeric gate on chapter number must treat 0 as valid —
audit pattern: grep `chapter < 1` / `ch >= 1` / `<= 0` next to chapter vars.
