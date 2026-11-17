---
type: Finding
title: comix WAF turnstile unsolvable in lightpanda headless; session ops notes
description: "comix.to /api/v1 403 captcha_required when the WAF cookie is absent; lightpanda headless cannot auto-solve the /@waf/challenge turnstile (widget #ts stays empty, status blank, even with --load-resources iframe). Recovery = seed cookies via --cookie from a real browser, or solve once in the user's chrome. Same-day hardening landed in 3f5f6a6: slow MCP tool calls return SSE frames (envelope_result reduces the last data: line); per-call timeout_ms rides in args (client fetch cap, 15s default); D13 recover probes first and keeps a healthy child instead of respawning (timeout != crash); run-cppIsekai.sh stop/restart reaps the app-owned lightpanda via graceful shutdown with an orphan sweep for the KILL path."
tags: [comix, waf, lightpanda, browser-host, lifecycle]
generated: { by: agent/mcp, at: "2026-11-17T02:42:52Z" }
status: stable
---


