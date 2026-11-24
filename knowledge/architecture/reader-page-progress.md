---
type: decision
title: "reader-page: four-mode reader UI + persisted positions (COMPLETE, archived 2026-11-10)"
description: "reader-page complete. /cache/img page-context 404 FIXED. chapter_number persists. Worker nudge carries WindowCtx (page_base chapter-absolute); rel_path gained manga/ segment -- one file per slot. [log] level knob added. 14/14 ctest. 4.1 browser smoke passed; change archived 2026-11-10; zoom (fit H/fit W/1:1) added 2026-11-10. 2026-11-15 UX pass: defaults ltr/fit-w, fixed split topbar, parity spread pairing (1,2)(3,4), polish; 18/18 tests, goldens re-locked."
tags: [reader-page, reader, position-persistence, cache-img-fix, logging, openspec, zoom, complete]
generated: { by: agent/mcp, at: "2026-11-24T13:14:04Z" }
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

## 2026-11-15 addendum: reader UX pass (defaults, topbar, spread pairing)
- Defaults: direction ltr + zoom fit-w, client state and C++ resume fallback aligned.
- Top bar fixed split: left [back to detail][mode][zoom group][dir]; right [page label (hidden <sm) + chapter select, title capped by formatChapterLabel (64 chars) + CSS truncate, container max-w-[50vw]].
- Spread pairing (h & v) is a pure parity grid (1,2)(3,4)...: the pair holding page p is (p,p+1) for odd p, (p-1,p) for even p; odd tail alone. Supersedes the fixed (1)(2,3) grid -- reader-page spec scenario 'page 1 renders alone' is thereby superseded (spec not re-edited; recorded here).
- Anchor-state approach (reanchor/_pairAnchor) discarded before landing: views stay a pure function of pages, no re-hook needed on mode change.
- Polish: bottom-sheet page select matches topbar focus/contrast; formatChapterLabel helper added (was referenced-only -- chapter options rendered empty without it).
- Verified: goldens re-locked (tests/fixtures/golden/reader.mustache.html), node --check on both rendered inline scripts, xmake test 18/18, daemon restarted (releasedbg :8090) /health ok.
- Related to [Every page mirrors its view state into the URL (reader parity, replaceState only)](shareable-url-state.md)
