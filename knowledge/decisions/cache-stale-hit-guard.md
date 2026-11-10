---
type: decision
title: "cache stale-hit guard: wipe no longer serves 200-blank"
description: "Manual cache wipe left the boot-scan index stale; img_ensure hit paths returned deleted paths -> 200-blank images, never refetched. Fix: fs::exists guard on all three hit paths (page ctx, sha fallback, cover_serve); stale entry dropped, falls through to fetch_, re-lands same v3 path. Offline-first preserved: present file never refetches (one stat per hit). Transcode-empty body no longer persists. alive() must run outside mu_ (std::mutex non-recursive): candidate chosen under lock, verified outside. Debug logs on all miss/failure paths. Locked by test_cache 'stale index entry after manual wipe falls through to refetch'. Commit bdd4d9f."
generated: { by: agent/mcp, at: "2026-11-10T08:15:38Z" }
status: stable
---


