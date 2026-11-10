---
type: Decision
title: Settings restart button + per-chapter CBZ cache-full gate
description: "Restart-settings-button via fork watchdog + execv, and per-chapter CBZ gate on cache-full probe."
tags: [http, settings, restart, cbz, cache]
generated: { by: agent/mcp, at: "2026-11-10T15:23:39Z" }
status: stable
---

Settings page gained a Restart server button. Implementation: `POST /admin/restart` forks a watchdog child that (1) closes fd>=3 to drop crow's listen socket, (2) polls `kill(parent,0)==ESRCH` (EPERM after reparenting is NOT death), (3) sleeps 1500ms for the port to free, (4) strips a trailing ` (deleted)` suffix from `/proc/self/exe` readlink (rebuilt binary since boot) and `execv`s it with the NUL-separated `/proc/self/cmdline` argv. Crow's `app.stop()` is called ~300ms after the response is flushed. Global top-banner (templates/bottom.mustache + reader_foot.mustache) polls `/health` (5s, 1s after user-triggered restart) and shows offline/restarting/online-again flashes. CBZ export gate: `/api/cbz-ready?plugin=&slug=&ch=&pages=` returns `{full:bool}` counting `data/cache/img/<plugin>/manga/<slug>/chNNN*/` files >= reported page total; detail page hides/disables the cbz button per chapter when the chapter is not fully cached. `/export/cbz` itself is NOT hard-gated (soft/UI gate only). Known footguns fixed: EPERM-vs-ESRCH, inherited listen fd, ` (deleted)` re-exec path; a failed re-exec left the live server down until manually restarted.
