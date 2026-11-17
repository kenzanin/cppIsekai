---
type: Finding
title: "Chrome/CDP browser backend: invariants, wait tool, and the comix tomy2x redeploy breakage"
description: "Chrome-headless-via-cdp-cpp replaced lightpanda: boot/mutex/orphan invariants, CDP wait tool, UA+exception hardening, and the comix frontend redeploy that killed DOM harvesting (API-mode harvest is the fix)."
tags: [browser-host, cdp, chrome, comix, harvest, wait-tool]
generated: { by: agent/mcp, at: "2026-11-17T12:56:11Z" }
status: stable
---

## Context

The lightpanda/MCP bridge was replaced by Chrome headless driven through the
vendored `third_party/cdp-cpp` (MIT, static lib). Plugin-facing
`host.browser.*` contract unchanged; `src/browser/chrome.{hpp,cpp}` (~230 loc)
is the whole backend.

## Hard invariants learned

1. **Boot mutex self-deadlock**: `call()` held the session mutex and then
   called `boot()` which locks the same non-recursive mutex. Split into
   `boot()` / `boot_locked()`; callers under the lock must use `_locked`.
2. **reproc retired**: chrome spawns via cdp-cpp; orphaned temp-profile
   Chromes (`user-data-dir=/tmp/cppisekai*`) are swept by
   `run-cppIsekai.sh restart` before boot. SIGTERM → `browser::shutdown`
   reaps the app-owned Chrome (killOnDestruct).
3. **Vendored patch**: `QuickStart.cpp` evaluate now passes
   `awaitPromise: true` (upstream never set it; comix harvest promises
   returned unresolved).
4. **UA override (cb7d84c)**: comix's router bounces `HeadlessChrome` UA off
   title pages. Boot applies `Emulation.setUserAgentOverride` (Headless→Chrome)
   once; log line `browser: ua override -> ...`.
5. **Exception funnel (cb7d84c)**: `Runtime.exceptionThrown` →
   `[browser] page exception: ...` at debug level, permanent.

## `wait` host tool (9ffc875)

`host.browser.call("wait", {expression, timeout_ms})` — CDP-backed
`!!expr` polling server-side. Comix harvest belt-wait was rewritten so the
wait expression itself absorbs visible rows each tick (the wait IS the
collector; no settle race, no const delays). Verified 100/111 chapters at
20.7 s cold before the site redeployed.

## comix frontend redeploy (bundle `main-tomy2x`, 2026-11-17 ~18:40)

Three stacked breakages of the DOM harvest:

- JS-location navigations (`location.href=`) bounce `/title/*` → `/`;
  CDP `Page.navigate` holds. (Probe via enginetest `location.href` misleads.)
- `HeadlessChrome` UA bounce — fixed by the override above.
- Chapter rows render ~2 s post-load then unmount, headed and headless
  alike; the sections' data XHR fires late (~20 s).

The data layer is fine: `GET
/api/v1/manga/<id>/chapters?page=N&limit=20&order[number]=desc&v=0&w=0&_=<token>`
returns 200 in-page; the rotating token (`etu.` prefix) is readable from
`performance` resource entries.

**Fix direction (next session)**: rewrite `ch_harvest` to API mode —
wait for the resource entry, strip `&page=`, in-page fetch-loop pages
1..N with that tokenized URL, map JSON rows to `{h,c,t}`. No DOM
dependency; immune to their frontend churn.

## Verification

- `xmake test` 19/19 after each landing; zero-warning project targets.
- comix e2e (pre-redeploy): chapters 111 → pages → `/cache/img` 200 image/jxl.
- UA override: path holds `/title/...` 2/2 smoke runs vs bounce without.

generated:
  by: opencode orchestrator
  at: 2026-11-17T19:55:00+07:00

# Related Concepts
- [browser-host apply findings (reproc reuse EINVAL, lightpanda await)](browser-host-apply-2026-11-17.md): Supersedes the lightpanda-era backend; its invariants (D13, single-session) carry over re-shaped.
- [comix turnstile token: how to mint the WAF cookie for headless harvest (DRAFT - for discussion)](../decisions/comix-turnstile-token-2026-11-17.md): Chrome swap made turnstile minting moot: real-Chrome engine solves the WAF transparently; decision left deprecated.
