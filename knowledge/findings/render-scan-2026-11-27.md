---
type: finding
title: "Plugin fleet scan 2026-11-27: 3 real breaks, 1 upstream outage, plain-TLS landmine"
description: "Full-plugin probe (43): madaradex search fixed (mdx_search), 4ucomics fixed (renderer wait 20s + body unwrap), valirscans search unrecoverable-without-host-fix -> removed from live 2026-11-27; comico/others flaky-but-alive."
tags: [scan, probe, madaradex, 4ucomics, valirscans, comico, fixed, parked]
generated: { by: agent/mcp, at: "2026-11-27T15:25:23Z" }
status: stable
---

# Method

Probe every enabled plugin's search->detail->chapters->pages via /plugin/<id>/*
(12-way parallel), then re-probe failures solo, then verify each break against
the live site with curl/plain and the impersonate binary.

# Fixed

1. madaradex search: the reader-theme redesign renders no result cards; the
   machine endpoint is admin-ajax action=mdx_search (JSON {data:{items}}).
   search.lua now uses it for query search; tag search keeps genre archives.
   Verified: dungeon=3 (= direct API), one=8, piece=2.
2. 4ucomics invalid JSON: lightpanda's hardcoded 8s wait now loses the CF race
   (needs 15-25s) and the failure page fed json.decode. fetch_render wait
   8s->20s + get_json strips a <body> wrapper too. Verified end-to-end 200.
3. comico 404s in my first probe were my own /api/v1 URLs; the plugin already
   uses /api. The reader-open 502s are an upstream outage (cf-ray SIN, 5/5) -
   the 4x retry already covers the intermittent shape.

# Parked

- valirscans search: impersonated clients get 307->/ (SPA shell) on /api/*;
  plain TLS returns JSON but the plain path segfaults the daemon
  (system easy_perform's internal curl_multi_* bind to the linked impersonate
   lib's unversioned symbols -> mixed-state crash in multi_poll). The
  host.http plain opt exists, builds and tests green, but is unused until the
  two-curl-lib pair is isolated (unique soname copy or both built in-process).
- 1manga no mhub_access: token config, user-side.
- Worker Crash remote_endpoint x6: crow noise on client aborts (my probe
  timeouts); harmless.
- store db-busy 15s giveup: only under my 12-way probe storm; single-user OK.

# Regression

22/22 ctest after the fetch_render + http-opt changes.

# Related Concepts
- [Renderer fetch engines: lightpanda/obscura profiles in the fetch gate](../decisions/renderer-fetch-engines.md): scan outcome
- [4ucomics plugin: private JSON API v1 behind Cloudflare, thin manga payload](../architecture/4ucomics-plugin.md): scan outcome
- [Plugin liveness scan: only kaliscan.me and anisascans.in are globally dead; most peers fail locally on ISP DNS interception](plugin-liveness-2026-11-19.md): same probe shape, earlier liveness pass
