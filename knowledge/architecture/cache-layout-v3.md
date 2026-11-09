---
type: decision
title: "cache-layout-v3: per-plugin layout, global transcode, host cover ratio"
description: "cache-layout-v3 COMPLETE 2026-11-09 (5/5 tasks, merged to main --no-ff, archived). V3 per-plugin layout img/<plugin>/cover/<hash16>.<ext> + img/<plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext> (min-3 pad, grows past 999); global transcode via [cache] image_format (webp default, vips buffer in/out) + cover resize to cover_max_px; host-computed ratio in JSON sidecar served as X-Image-Ratio. Key wiring fix: /cache/img previously bypassed img_store/transcode entirely -- new Cache::img_ensure is the single wrap point (hit = extension-free lookup so mid-flight format flips keep serving; miss = fetch -> transcode -> v3 path + sidecar). Live smoke: 1.79MB png -> 103KB webp; flip image_format mid-run keeps old files serving. ctest 12/12, zero warnings."
tags: [cache-layout-v3, cache, transcode, vips, webp, sidecar, cover-ratio, merged, archived]
generated: { by: agent/mcp, at: "2026-11-09T11:45:17Z" }
status: stable
---

# cache-layout-v3: per-plugin layout, global transcode, host cover ratio

COMPLETE 2026-11-09 (5/5 tasks, merged to main --no-ff, archived as 2026-11-09-cache-layout-v3). ctest 12/12, zero warnings, openspec validate 9/9.

## Layout
- Covers: `img/<plugin>/cover/<sha1(url)-16>.<ext>` + sidecar
- Pages: `img/<plugin>/manga/<slug>/ch<n>-<id8>/pNNN.<ext>` (min-3 pad, grows past 999 safely)
- No context: v2 sha fallback unchanged

## Transcode + ratio
- Global `[cache] image_format` (webp default; avif/jxl/none) via vips buffer in/out (`src/cache/transcode.hpp`)
- Covers resized to `[cache] cover_max_px` (default 800)
- Host-computed ratio in `<file>.json` sidecar {w,h,ratio}; served as `X-Image-Ratio`; client ratio params ignored

## Key wiring decision
`/cache/img` previously bypassed img_store/transcode entirely (unit tests green, live path raw). New `Cache::img_ensure` is the single wrap point: hit = extension-free lookup so mid-flight format flips keep serving old files; miss = fetch -> transcode -> v3 path + sidecar. Config knobs plumbed into `cache::Config`; VIPS linked into every target compiling cache.cpp.

Live smoke: 1.79MB png -> 103KB webp with sidecar; image_format flipped to none mid-run -> old webp still served.

# Related Concepts
- [cppIsekai image-cache + offline-first pathway](image-cache-offline.md): Supersedes/extends: v3 reorganizes the v2 image pool into per-plugin layout and adds global transcode + host-computed cover ratio
