---
type: Constraint
title: "host.http.fetch* writes body only on HTTP 200 (wasm imports)"
description: "src/plugin/wasm/wasm_imports.cpp http_fetch_body/json_fetch only write to the guest buffer when the host reply has status == 200; every other status collapses to kNotFound and the guest sees a silent failure, never a code."
tags: [wasm, wsam, host, fetch, http, constraint]
generated: { by: agent/mcp, at: "2026-11-16T00:38:58Z" }
status: stable
---

src/plugin/wasm/wasm_imports.cpp (verified 2026-11-16): `http_fetch_body` and `http_json_body` parse the host `http_fetch` reply; `is_200` is the ONLY body-write path. Any non-200 (or unparseable JSON reply, or missing body) returns `kNotFound`. The guest wrapper `fetch()` in plugins_dev/wasm/* coerces that to `false`/`0` with no status code, so:

- A 403/302/500/challenge page is indistinguishable from a missing page to the guest.
- A guest that writes `if (!fetch(url, opts)) break;` fails silently with no log line.
- The daemon log at info level shows nothing; only `debug` level would help and only for cache hits.

Same shape as the existing pitfalls #4/#9 in references/wasm-guest-pitfalls: oversized => kTruncated => silent zero; non-200 => kNotFound => silent break. Rule for guests: never break silently — emit reply_err(...) or a host.log marker so the failure is visible.

Related: references/wasm-guest-pitfalls.md, decisions/fetch-profile-gate.md.
