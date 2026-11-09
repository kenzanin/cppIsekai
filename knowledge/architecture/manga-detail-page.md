---
type: decision
title: "manga-detail-page: stale-serve + fail-safe refresh pattern"
description: "Detail page pattern: stale-serve detail/chapters via existing call_cached, fail-safe POST refresh that overwrites both stale entries only after both envelopes validate, SSR shell + Alpine hydrate, per-chapter CBZ links, slug guard 404."
tags: [detail-page, stale-serve, refresh, cbz, alpine, ssr]
generated: { by: agent/mcp, at: "2026-11-09T03:01:44Z" }
status: stable
---

Decision (change manga-detail-page, 6/6 tasks, committed 97b5f2e..36f4b09 on manga-detail-page branch):

SCOPE. Read-only detail page at /view/manga/<plugin>/<slug>: metadata + chapter list + per-chapter CBZ link + manual refresh. NO cover management (deferred).

ENDPOINTS (D2, stale-serve). GET /api/detail/<plugin>/<slug> and GET /api/chapters/<plugin>/<slug> reuse the existing call_cached wrap (src/http/stale.hpp) - detail/chapters were already stale-optin, zero stale.hpp changes. Shared helper title_data_response: slug guard -> enabled-gate -> on_pool call. Slug guard valid_title_pair: empty, >128, path separators, dot-segments => 404 {"ok":false,"error":"unknown title"}; ponytail: full revalidation deferred to the cover-management change.

REFRESH (D3, fail-safe). POST /api/refresh/<plugin>/<slug> is call-through both methods (detail then chapters, sequential), overwrites the stale entries only after BOTH envelopes validate, using the SAME key shape call_cached persists under (stale_key(plugin, method, params) + cache.store(key,"api",body)); first failure returns {"ok":false,"error":"refresh failed: <method>"} and leaves every cached entry untouched. Note: {"ok":false,"error":"refresh failed: detail"} - verify the server actually returned 200 on the failure path (spec wants 502/503 on plugin failure; the smoke showed [200]) - fix before archive or amend the spec.

PAGE (D1, SSR shell + Alpine). Route renders a server-side shell (detail.mustache, layout library); Alpine component detailPage() rebuilds API URLs from location.pathname, hydrates detail+chapters, degrades per-field, shows cached badge from stale envelopes, chapter rows = Read link (/reader?plugin=&id=) + cbz link (/export/cbz?plugin=&slug=<slug>%2FchNNN). Covers via /cache/img?url=encoded (subprocess fetcher rationale: uploads.mangadex.org TLS dies against the in-process impersonate libcurl - see references/uploads-cdn-tls-constraint).

SMOKE (verified live): page 200 + detailPage mounted; refresh {"ok":true}; detail/chapters cached:true under bogus proxy (proxy is boot-time, NOT live-swapped by POST /config - only fetch_profile swaps live); refresh failure path cache-intact; guard 404; ctest green throughout; openspec validate 2/2.

# Related Concepts
- Related to [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md)
- Related to [cppIsekai image-cache + offline-first pathway](image-cache-offline.md)
