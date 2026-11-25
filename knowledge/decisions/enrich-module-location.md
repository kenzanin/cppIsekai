---
type: Decision
title: "enrich module lives in src/enrich/, http/ = routes + shaping"
description: "Enrichment domain logic lives in src/enrich/, not src/http/; http/ keeps routes and response shaping only."
tags: [http, enrichment, structure, refactor]
generated: { by: agent/mcp, at: "2026-11-25T15:35:13Z" }
status: stable
---

## Decision
`src/enrich/` owns enrichment domain logic (candidate resolution, retry chains, source/chosen/detail). `src/http/` is routes + response shaping only; new domain logic does not go there.

## Mechanics
- 9 files moved (enrich_{chosen,chosen_helpers,chosen_lookup,detail,source,source_retry}.cpp + 3 internal.hpp); namespace `http::api` unchanged -> zero call-site churn.
- 10 include sites updated (5 http_api_*, 4 http_pages_*, api_ctx.hpp pragma-once cycle, tests/test_http_enrich.cpp).
- xmake: ENRICH_SRC paths; daemon target globs `src/**.cpp` so unpicked. Full suite 22/22 pass (80b680d).

generated: { by: "kit-agent", at: "2026-11-25" }

# Related Concepts
- [Detail page design + enrichment plugin (alt cover/alt info)](../architecture/detail-page.md): enrich module implements the detail-page enrichment flow moved out of http/
