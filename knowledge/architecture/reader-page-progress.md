---
type: decision
title: "reader-page: four-mode reader UI + persisted positions (COMPLETE, archived 2026-11-10)"
description: "reader-page complete. /cache/img page-context 404 FIXED. chapter_number persists. Worker nudge carries WindowCtx (page_base chapter-absolute); rel_path gained manga/ segment -- one file per slot. [log] level knob added. 14/14 ctest. 4.1 browser smoke passed; change archived 2026-11-10; zoom (fit H/fit W/1:1) added 2026-11-10."
tags: [reader-page, reader, position-persistence, cache-img-fix, logging, openspec, zoom, complete]
generated: { by: agent/mcp, at: "2026-11-10T00:51:34Z" }
status: stable
---

## State
reader-page COMPLETE. Spec synced to openspec/specs/reader-page/spec.md; archived to openspec/changes/archive/2026-11-10-reader-page. Executed directly on main (72a1268..0d8454a), no --no-ff branch merge, contrary to AGENTS.md branch convention.

## Key facts
- Reader route: `/read/<plugin>/<slug>?ch=<n>`; `/view/manga/...` is the detail shelf. Detail.mustache path parse was off-by-one (plugin=parts[2], slug=parts.slice(3)); fixed 2026-11-10, root cause of the 'detail page still error' report.
- Resume-after-restart verified: ch 1 p 7 spread-h rtl restored with no URL params.
- Zoom added 2026-11-10, initially client-only: fit H (default, max-h-full object-contain, tap zones active), fit W (w-full / spread-h w-1/2, shell scrollable overflow-auto, tap zones hidden), 1:1 (natural size max-w-none, scrollable, tap zones hidden). Keyboard nav works in all modes. Zoom persistence + full-screen chrome landed in reader-fullscreen (see architecture/reader-fullscreen.md): additive reader_positions.zoom column, chUrl carries mode/dir/zoom cross-chapter, resume_zoom seeded on /read.
- Position POST schema keys: plugin, manga (slug), chapter (chapter ID), chapter_number (number), page, mode, direction. /api/pages takes chapter NUMBER; reader resolves number->ID client-side via /api/chapters.
- Worker v3 placement: WindowCtx{plugin, slug, chapter, page_base}; page_base>0 = chapter-absolute; empty plugin => sha fallback. rel_path shape = cache::page_rel (<plugin>/manga/<slug>/chNNN/pNNN.<ext>).
- MangaDex quirk: some chapters (Solo Leveling) return empty at-home data[]; daemon correctly errors, not a bug. Working test manga: Kage no Jitsuryokusha 77bee52c-... ch1 (37 pages); Isekai Meikyuu de Harem o 1389d660-... ok.

## Facts / gaps
See references/reader-page-gaps.

# Related Concepts
- [reader-page: gaps and important facts (post-archive 2026-11-10)](../references/reader-page-gaps.md): reader-page decision references the gaps/facts backlog from the same session (skip off-by-skip, legacy cn=0 rows, MangaDex empty at-home data[], main-branch commits).
- [reader-fullscreen: full-screen reader chrome + zoom persistence](../architecture/reader-fullscreen.md): closes the zoom-persistence gap and adds Done overlay, overlay nav, keyboard/wheel fixes.
