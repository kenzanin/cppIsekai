---
type: decision
title: "Plugin cookie jars: libcurl-native, per-plugin file state"
description: "User approved 2026-11-08: native libcurl cookie engine (CURLOPT_COOKIEFILE/JAR), one jar per plugin at data/plugin-state/<id>.cookies.txt, enabled via a fetch-gate option; no new dependencies; implementation lands in change fetch-gate."
tags: [cookie, curl, plugin-state, fetch, security]
generated: { by: agent/mcp, at: "2026-11-08T09:00:00Z" }
status: stable
---

Answer to "do we need a library for cookie jars?" - **no**; libcurl ships the engine.

## Policy

- **Engine**: `CURLOPT_COOKIEFILE` (jar path; empty string = enable without loading) +
  `CURLOPT_COOKIEJAR` (Netscape-format persist on cleanup). Inspection via
  `CURLOPT_COOKIELIST`/`CURLINFO_COOKIE_LIST`.
- **Location**: `data/plugin-state/<id>.cookies.txt` - deliberately under data/, NOT the
  plugin folder (plugin folders are read-only per
  `decisions/plugin-data-segregation`; state is separated by design).
- **Isolation**: a fresh curl handle per request + loading only its own jar => per-plugin
  isolation for free; persists across restarts automatically.
- **Control**: a `cookie_jar` (path) option on `fetch::Request` - the gate sets it up;
  empty = cookie engine off (today's behavior).

## Relevance

Mangadex uses `storage.*` for OAuth tokens; this jar covers sites that depend on session
cookies (CF clearance etc.). Implementation lands in change `fetch-gate` (same choke
point as the impersonate gate).

## Related

- [Plugin data segregation: host .ini is host-only](plugin-data-segregation.md): Why the jar lives in data/, not the plugin folder.
- [Fetch profile gate: centralized curl-impersonate, dynamic per-call profile](fetch-profile-gate.md): The gate that mounts the cookie engine.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): Plugin opts gaining the cookie field.
