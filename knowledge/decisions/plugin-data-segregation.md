---
type: decision
title: "Plugin data segregation: host .ini is host-only"
description: "Where plugin data lives: host .ini is host-only config; static manifest in main.json; user credentials in gitignored config.json; dynamic state (OAuth tokens) via per-plugin storage host tool backed by the DB."
tags: [plugin-system, auth, mangadex, config]
generated: { by: agent/mcp, at: "2026-11-07T01:14:33Z" }
status: stable
---

## Rule

- `data/cppisekai.ini`: **host-only** config (bind ip/port, image cache format, max cover resolution, `fetch_timeout_ms`, plugins dir). Plugins never read or write it.
- `data/plugins/<id>/main.json`: static manifest — identity, capabilities, api version, non-secret defaults. Read by loader only.
- `data/plugins/<id>/config.json`: user credentials/settings (e.g. MangaDex personal client id/secret, username, password). Not read by the loader; gitignored; read at runtime via a host tool (`config.get`).
- Dynamic state (OAuth access/refresh tokens): host-provided key-value storage host tool (`storage.get`/`storage.set`) scoped per plugin_id, backed by the DB; survives restart.

## Why
MangaDex added OAuth2 (auth.mangadex.org, password flow for personal clients; 15-min access tokens, refresh flow) which requires POST form-urlencoded credentials and persistent token state. Sandbox has no `io.write` by design, so plugins cannot persist anything themselves; the loader must never see secrets, and `main.json` must stay a stable distributed artifact.

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): MangaDex auth (config.json credentials + storage-backed tokens) is realized through plugin host tools defined here.
