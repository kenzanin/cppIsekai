---
type: reference
title: MangaDex auth and API quirks (verified)
description: "Verified MangaDex auth/API behaviors: password-grant-only personal clients, public endpoints tolerating any bearer, no auth headers on CDN, impersonate TLS flakiness, token state in plugin_state."
tags: [mangadex, auth, quirks]
generated: { by: agent/mcp, at: "2026-11-07T03:04:36Z" }
status: stable
---

## Facts (verified live 2026-11-07)

- Personal client = `password` grant only (public clients unreleased): POST form-urlencoded to `auth.mangadex.org/realms/mangadex/protocol/openid-connect/token` with client id/secret + user/pass -> access (~15 min) + refresh token.
- Public API endpoints tolerate ANY bearer token — a garbage `Authorization` header still gets 200 on `GET /manga`. Auth headers matter only for authenticated endpoints; sending them unnecessarily just breaks caching.
- MangaDex image/CDN hosts must NOT receive auth headers.
- curl-impersonate (chrome profile) intermittently fails TLS vs MangaDex LB (ECONNRESET); one fresh-handle retry clears it (implemented in `http.fetch`).
- Tokens live in DB `plugin_state(plugin_id,key,value)` via `storage.get/set`; refresh on expiry or 401-once, fallback full re-login when the refresh token is rejected.

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): MangaDex quirks constrained the mangadex plugin auth design and the storage host tools.
