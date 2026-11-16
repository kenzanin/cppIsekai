---
type: Finding
title: "browser-host apply findings (reproc reuse EINVAL, lightpanda await)"
description: reproc++ process reuse EINVAL on respawn; lightpanda evaluate awaits promises; both verified on the D13/comix path
tags: [browser-host, reproc, lightpanda, comix]
generated: { by: agent/mcp, at: "2026-11-16T18:05:02Z" }
status: stable
---

## Finding

Two empirical facts from applying `openspec/changes/browser-host` (2026-11-17):

1. **reproc++ process reuse EINVAL.** Calling `start()` a second time on a
   `reproc::process` that already ran a child to completion returns
   `EINVAL` ("spawn 'lightpanda' failed: Invalid argument") — the D13 crash
   path failed instantly while the boot path (fresh object) worked. Fix:
   assign a fresh `reproc::process{}` per spawn in `src/browser/supervise.cpp`
   and track liveness with a bool instead of `pid()`.
2. **lightpanda MCP `evaluate` awaits promises.** A single `evaluate` whose
   script returns a Promise resolves to the fulfilled value (verified:
   300 ms timer → `"awaited-2"`). The whole IntersectionObserver harvest
   therefore fits ONE call: poll for `.rpage-page`, scroll each slot
   (150 ms), collect `img[src]`, resolve JSON. No `waitForSelector` chain
   needed.

## Verification

- kill → call → `browser: lightpanda respawned (D13)` → data ok; exactly one
  new `lightpanda mcp` child (pgrep), previous instance a reaped zombie.
- comix e2e: `/api/pages/comix/.../22.5?ch_id=...` → 6 host `/cache/img` URLs;
  `GET /cache/img?...` → 200 image/jxl (494 KB) stored under
  `ch022-ter-22.5/p001.jxl`; headless reader render shows the `/cache/img`
  `img[src]` set.
- `xmake test` 19/19; `openspec validate browser-host` valid.

generated:
  by: opencode orchestrator
  at: 2026-11-17T01:10:00+07:00
