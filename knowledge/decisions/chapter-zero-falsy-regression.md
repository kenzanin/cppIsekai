---
type: Decision
title: "reader: chapter 0 is valid — falsy-zero regression fixed + unit-locked"
description: Chapter 0 (prequel) and fractional chapters (1.5) were dropped by JS/C++ truthiness checks on 0 after edge chapter lists started serving ch 0. Fixed in f1ce761; regression locked by test_pages 383c257.
tags: [reader, ch0, falsy, pages-api, test_pages]
generated: { by: agent/mcp, at: "2026-11-10T11:34:03Z" }
status: stable
---

Symptom: /read?ch=0 opened chapter 1 or the stored resume chapter, never chapter 0 (manga mangadex 1389d660 has ch 0). Root causes: (1) reader.mustache boot used Number('{{resume_ch}}') || 1 (0 -> 1) and `if (qCh)` (0 discarded); (2) src/http/http_pages.cpp clamped resume ch with std::max(1, ...) (0 -> 1); (3) /reader/position used chapter_number > 0 / <= 0 (0 meant absent). Fix: Math.max(0, ...) + Number.isFinite gates client-side; std::max(0, ...) server-side; /reader/position now carries chapter_number as std::optional<double> (absent keeps stored, 0 is a real number). Route /api/pages moved from <int> to <double> in 3304fe2 for fractional chapters; fmt_ch emits integral 2 (not 2.000000) and 1.5 untrimmed. Unit lock: tests/test_pages.cpp "pages: chapter 0 is a valid chapter (falsy-zero regression)" + fractional 1.5 case; ctest 14/14. Related: architecture/reader-readahead, architecture/reader-page-progress.
