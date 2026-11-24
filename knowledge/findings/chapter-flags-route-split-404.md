---
type: Finding
title: TU split silently dropped POST /chapter/flags (404) -- route-surface test added
description: "2026-11-24: POST /chapter/flags returned bare 404 from Crow; the detail page's mark-prev/clear-next and curl callers of the generic bitfield batch endpoint were dead. Root cause: commit 522d800 (src/http TU split) deleted the CROW_ROUTE registration from http_api_state.cpp without adding it to http_api_state_flags.cpp -- the only route lost in that split (route-literal diff before/after). Restored verbatim in http_api_state_flags.cpp including the immediate saved-row read rollup. Added tests/test_routes.cpp target: asserts nine curl-facing CROW_ROUTE literals still exist under src/http, so a future split cannot drop one silently. Verified with the user's exact curl (200 with flags object, set then clear); restored that title's pre-probe 80/93 read flags from the 11:45 backup afterwards."
tags: [route, chapter-flags, 404, refactor-regression, test, fixed]
generated: { by: agent/mcp, at: "2026-11-24T14:21:18Z" }
status: stable
---

# Related Concepts
- Related to [Source layout: crown facades, ~200-loc units, clang-doc comments](../decisions/source-layout-policy.md)
