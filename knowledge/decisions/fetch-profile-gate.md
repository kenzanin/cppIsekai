---
type: decision
title: "Fetch profile gate: centralized curl-impersonate, dynamic per-call profile"
description: "User steer 2026-11-08: move the impersonate gate to src/fetch/ (single call site); per-call profile from plugin opts > fetch_profile ini; live default via POST /config; linked-lib targets chrome99-116/edge99-101/safari15; firefox = phase 1 via dual-.so dlopen vtable (steer 2026-11-08); hand-rolled JA3 profile-class rejected."
tags: [fetch, curl-impersonate, plugin-api, config, performance]
generated: { by: agent/mcp, at: "2026-11-08T08:30:00Z" }
status: stable
---

This decision executes the user direction from the `tmp/curl-impresionate.md` discussion (a BrowserProfile demo): dynamic profile switching WITHOUT a hand-rolled JA3 class.

## D1 - Centralized gate in src/fetch/

`src/fetch/fetch.hpp/.cpp` = the only `curl_easy_impersonate` call site in the tree.
`fetch::Request{url, timeout_ms, profile?, method, body, headers...}` +
`fetch::sync(Request, proxy) -> {status, body, headers, error}`. `fetch_sync` +
`applyProxy` + the sinks move out of plugin.cpp; the duplicate call sites (:261 retry &
:562 legacy lua) disappear with it. The existing ponytail retry is preserved in the gate.

## D2 - Profile resolution & live default

Precedence: `opts.profile` (per-call, from lua/js/wasm plugins) > `plugin.fetch_profile`
ini. Live default: config is read at call time (shared snapshot), not captured in
closures at load -> `POST /config plugin.fetch_profile=...` applies to subsequent calls
without restart/reload.

## D3 - Targets & families

The linked lib (`libcurl-impersonate-chrome.so.4.8.0`, v0.6.1) accepts: chrome99/100/101/104/107/110/116,
edge99/101, safari15_3, safari15_5 (strings probe; no plain safari15). Firefox (user steer 2026-11-08,
same phase): the sibling `libcurl-impersonate-ff.so.4` (NSS build, v0.6.1, vendored next to the chrome
lib) loads `dlopen(RTLD_LOCAL)` behind a per-family symbol vtable - one gate, two independent lib
copies; ff*/firefox* targets route to it, the ff set probed from the lib:
ff91esr/95/98/100/102/109/117. Shipped in change fetch-gate 6/6 (ctest 10/10); the absent-lib path
degrades to the chrome engine instead of failing the request.

## D4 - Rejected: manual profile class

Per-browser UA/headers/ciphers/EC-curves (demo style) will not be implemented -
`curl_easy_impersonate` already bakes all of it per target; a class would only drift
from the lib's fingerprint.

## Follow-ups (outside this change)

Persistent per-plugin cookie jars and the search read-model cache - see the 2026-11-08
decisions; each is its own decision/change.

## Related

- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](deps-and-plugins.md): The curl-first ruling this gate implements.
- [cli-config: registry-backed config surface](../architecture/cli-config.md): Owner of the `plugin.fetch_profile` knob used as the fallback default.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): The ABI whose opts gain a `profile` field.
