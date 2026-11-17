---
type: decision
title: "comix turnstile token: how to mint the WAF cookie for headless harvest (DRAFT - for discussion)"
description: "DRAFT, pending discussion. Last unsolved piece: /api/v1 needs the site's waf cookie, set only by POST /@waf/verify with a real turnstile token. Flow mapped: challenge #ts shell renders, iframe does NOT auto-mount, turnstile.reset('#ts') mounts it, token lands in input[id$=_response], glue auto-POSTs verify. Fails headless: token never completes in lightpanda (response input empty >25s). Options: A) solve once in user Chrome, export to jar, --cookie loads at boot until expiry (zero code); B) drive user Chrome via Blueprint/CDP to re-solve automatically; C) keep digging lightpanda turnstile internals (one nightly crash seen; upstream-report candidate). Lean A now, B if manual re-solve annoys, C background only. Evidence: findings/browser-session-waf-2026-11-17."
tags: [comix, turnstile, waf, lightpanda, decision]
generated: { by: agent/mcp, at: "2026-11-17T03:13:45Z" }
status: stable
---

## Context

comix reader chain is complete except one thing: `/api/v1/*` answers `403 {"error":"captcha_required","challenge":"/@waf/challenge"}` unless the site's own WAF cookie is present. That cookie is set **only** by `POST /@waf/verify` with a real Cloudflare Turnstile token. The whole flow is now mapped (see `findings/browser-session-waf-2026-11-17`):

1. Challenge page renders the `#ts` shell; the challenge iframe does **not** auto-mount on load.
2. `turnstile.reset('#ts')` mounts it (verified: iframe count 0 -> 1).
3. On success the token lands in `input[id$=_response]`; the page glue auto-POSTs `/@waf/verify` and the site cookie is set.

What fails headless: the token never completes in lightpanda - response input still empty after >25s with the iframe mounted.

## Options

| | Path | Cost | When |
|---|---|---|---|
| A | Solve once in the user's real Chrome, export cookies to the jar; `--cookie` (already wired) loads them at every boot until expiry | zero new code; manual re-solve on expiry | now |
| B | Drive the user's Chrome via Blueprint/CDP to solve + scrape the cookie automatically on expiry | automated, but couples daemon flow to a running desktop Chrome | if manual re-solve annoys |
| C | Keep reverse-engineering lightpanda turnstile internals; nightly crashed once mid-probe - upstream-report candidate at lightpanda-io/browser | uncertain payoff, timebox already burned once | background only |

## Leaning

A now, B if the manual re-solve cadence hurts, C only as a filed issue upstream. The cookie persist loop (graceful stop -> jar -> boot load) makes A's expiry the only manual touchpoint.

