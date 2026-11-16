---
type: Finding
title: "Reader prefetch spill wrote next-chapter pages into the current chapter dir (cached>total)"
description: "Resolved: reader::next_window spilled into the next chapter but WindowCtx stamped the whole batch with the CURRENT chapter number/page_base, so chN dir accumulated p(N+1).. byte-dupes of chN+1 p1.. -- the detail counter (max file count) read cached>total. Fix: window_parts returns per-chapter slices (chapter_id/number/first_page); Worker::nudge lands each part in its own chapter dir; one-off prune removed 56 dupe files. Verified 7/7 chapters cached==total and a forced spill-miss landing ch003/p001 with ch002 untouched."
tags: [reader, prefetch, cache, window, cppIsekai, resolved]
generated: { by: agent/mcp, at: "2026-11-16T15:52:17Z" }
status: stable
---


