---
type: Fact
title: "reader-page: gaps and important facts (post-archive 2026-11-10)"
description: "Gap list operational facts reader-page session: zoom persistence gap CLOSED by reader-fullscreen, resume semantics, skip off-by-skip, legacy cn=0 rows, MangaDex empty data[] quirk."
tags: [reader-page, gaps, backlog, zoom]
generated: { by: agent/mcp, at: "2026-11-10T03:18:19Z" }
status: stable
---

## Gaps
- ~~Zoom (fit H / fit W / 1:1) client-only, not persisted~~ CLOSED 2026-11-10 by reader-fullscreen: additive `zoom` column on reader_positions (guarded ALTER TABLE), merge_position keep-stored, /api/position round-trip, reader seeds/posts zoom, chUrl carries it cross-chapter.
- Worker skip-window page_base off-by-skip: skip flags shift page numbering; dense numbering keeps reading order but page indices can drift from the /cache/img query-page convention.
- Legacy position rows without chapter_number (cn=0) never heal until the next POST; resume hint then shows ch 1 until read.
- chapter-relative vs chapter-absolute semantics: /api/pages takes chapter NUMBER; position store keys chapter ID. Reader resolves number->ID via /api/chapters on every boot (extra round trip).
- MangaDex upstream: chapters `pages` attribute present in /feed but cached `/api/chapters` envelopes predate the `pages` field (natural cache expiry heals; prev-boundary falls back to page 1). Some chapters return empty at-home data[]; not recoverable client-side.
- Branch convention broken: work committed directly on main, not change-id branch + --no-ff (AGENTS.md); reader-fullscreen also implemented on main, uncommitted.
- static/*.min.js.br committed but unreferenced in src/templates (Crow static handler serves /static/<path> as-is; no brotli content-negotiation observed).
- Wheel: fixed overlay bars ate native wheel events so scrollable image shell never scrolled; fixed 2026-11-10 by manual shell scroll in wheel handler (fit-w/1:1) / fit-h step-on-wheel / webtoon native body scroll.
- Daemon for manual testing: CPPISEKAI_LOG_LEVEL=debug ./build/cppIsekai --config data/cppisekai.ini --port 8090
- Test manga: 1389d660-b9b1-4c6a-81af-eab4dbf3f22b (Isekai Meikyuu de Harem o), all chapters have covers and details load; 77bee52c-d2d6-44ad-a33a-1734c1fe696a (Kage no Jitsuryokusha) ch1 37 pages.
- openspec validate --specs: 10 passed 0 failed. ctest 14/14.

# Related Concepts
- [reader-fullscreen: full-screen reader chrome + zoom persistence](../architecture/reader-fullscreen.md): closes the zoom-persistence gap; this file tracks the rest.
