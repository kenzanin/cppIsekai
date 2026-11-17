---
type: Finding
title: comix WAF turnstile unsolvable in lightpanda headless; session ops notes
description: "comix.to /api/v1 403 captcha_required when the WAF cookie is absent. Turnstile flow mapped: widget renders #ts shell but challenge iframe never mounts on load; `turnstile.reset('#tf')` mounts it, token lands in input[id$=_response], then POST /@waf/verify sets the site cookie. Nightly 12226 crashed once mid-probe (zombie, parent=daemon); D13 recovered on next call (respawn+retry, 10.9s incl. respawn). cf_clearance gets set by page nav but the site's own waf cookie needs the verify step. Recovery options: seed --cookie from a real browser, or solve once in user chrome. Ops hardening from the official skill pass: nightly 12226 deployed (was 12116); --cookie <jar> added next to --cookie-jar so graceful stops persist cookies for the next boot; goto response notes name exactly which requests were 403/429-refused and challenge-blocked (parse them); waitForSelector/evaluate-with-url builtins replace hand-rolled sleep loops."
tags: [comix, waf, lightpanda, browser-host, lifecycle]
generated: { by: agent/mcp, at: "2026-11-17T02:58:59Z" }
status: stable
---


