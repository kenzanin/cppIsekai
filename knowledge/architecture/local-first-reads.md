---
type: Requirement
title: Library reads are local-first
description: Library data always served from local db/cache when a row exists; website only on cold miss or explicit user refresh
generated: { by: agent/reasonix, at: "2026-11-10T14:59:00Z" }
status: stable
---

Local-first Library reads (2026-11-10). The app MUST serve Library/detail/chapters/pages/related from the local db/cache whenever a row exists in api_cache; the plugin/network is touched only on cold miss (nothing cached) or on an explicit user Refresh/Update. Never auto-refresh on TTL expiry. `related` was added to stale_optin and cached rows always win; expired rows are served with cached:true. Rationale: upstream sites must not be hammered on every page open. Implemented in src/http/stale.hpp (call_cached) + test_stale; see log 2026-11-10.
