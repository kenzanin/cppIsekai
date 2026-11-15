---
type: decision
title: "Renderer fetch engines: lightpanda/obscura profiles in the fetch gate"
description: "CF-challenge sites get headless-renderer fetch via profile 'lightpanda' or 'obscura' in fetch::sync (GET only, popen + coreutils timeout, status pinned 200, html into the normal lexbor path; non-GET falls back chrome116). 4ucomics: lightpanda solves (89KB real); toonkotsu home ok everywhere but inner pages stall post-verify in BOTH lightpanda and obscura-stealth (challenge phase captured, retries dont help) - parked until a real turnstile path exists. Images stay on the curl/vips path, never the renderer."
tags: [fetch, cloudflare, lightpanda, obscura, anti-bot, plugin-api]
generated: { by: agent/mcp, at: "2026-11-15T18:36:27Z" }
status: stable
---


