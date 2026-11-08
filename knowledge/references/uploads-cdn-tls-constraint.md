---
type: reference
title: "uploads CDN TLS constraint: system curl CLI is the only working engine"
description: uploads.mangadex.org rejects both impersonate TLS stacks (SSL_CONNECT_ERROR) and a second in-process libcurl segfaults; image fetching therefore shells out to the system curl CLI. Includes the UI cover path via /cache/img and two build lessons.
tags: [tls, curl-impersonate, anti-bot, crash, image-cache, constraint]
generated: { by: agent/mcp, at: "2026-11-08T22:58:10Z" }
status: stable
---

Verified 2026-11-09 during the search-cover e2e session (commit 6a7da4d):

1. TLS FACT: both vendored curl-impersonate stacks (chrome BoringSSL fork AND ff/NSS) fail with CURLE_SSL_CONNECT_ERROR against uploads.mangadex.org's CDN; the system libcurl (OpenSSL, curl CLI) always succeeds (200, JPEG). api.mangadex.org works fine on the chrome lib -- only the uploads CDN rejects those TLS stacks.
2. PROCESS FACT: two libcurl instances cannot coexist in-process here. dlopen'ing system libcurl.so.4 (even with curl_global_init) segfaults the process on first easy_perform; curl CLI works at all times.
3. SHIPPED SOLUTION: image-cache fetcher (makeCurlFetcher in src/http/http.cpp) shells out to `curl -sfL --max-time 30` (popen), -f so HTTP errors are never cached. Everything else (plugins, api pulls) stays on the gate. Upgrade path: dedicated out-of-process fetch helper if more than images ever needs system-TLS.
4. UI: search grid covers load via GET /cache/img?url=<encoded> (templates/search.mustache), so browser only talks localhost and covers are disk-cached -> visible offline.
5. BUILD LESSONS: `ff*/firefox*` inside a /** */ comment closes it early (`*/` in text!) -- compile error cascade far from the cause; missing include for LiveProfile in lua/bridge.hpp broke two TUs. Both fixed.

Related: decisions/fetch-profile-gate.md (gate), architecture/image-cache-offline.md (cache).

# Related Concepts
- Related to [Fetch profile gate: centralized curl-impersonate, dynamic per-call profile](../decisions/fetch-profile-gate.md)
- Related to [cppIsekai image-cache + offline-first pathway](../architecture/image-cache-offline.md)
