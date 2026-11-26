---
type: Decision
title: "titlestate: domain layer split out of src/http"
description: Title-state domain layer (envelope parse/chapter materialize/stat sync/refresh driver) moved from http/api_ctx to src/titlestate.hpp; scheduler no longer routes through http.
tags: [refactor, layering, http, title-state]
generated: { by: agent/mcp, at: "2026-11-26T07:10:48Z" }
status: stable
---

## Context
`src/http/api_ctx.hpp` grew to 444 lines: route Ctx + request-field readers, plus ~330 lines of domain logic (envelope parsers, chapter materialization, saved-row stat sync, `refresh_title`). `src/update/scheduler.cpp` included the **http** header just to reach `refresh_title` — domain logic reachable only through the HTTP layer, against `decisions/enrich-module-location`'s layering rule.

## Change (`d607d80`)
New **`src/titlestate.hpp`** (header-only): the title-state domain layer moved verbatim out of api_ctx —
- `namespace detail` envelope parsers (`envelope_object/str/raw_json`, `inject_status_flags`)
- `materialize_chapters`, `chapters_row_response`
- `apply_read_rollup`, `sync_saved_stats`
- `refresh_error_text`, `RefreshResult/RefreshOutcome`, `refresh_title`

Dependencies: `http::on_pool` (http_routes), `stale.hpp` (kApiCacheTtl/envelope_data/stale_key), store/plugin, and a forward decl of `http::api::valid_title_pair` (definition stays in api_ctx; every consumer TU also includes api_ctx). `api_ctx.hpp` shrinks 444 -> 114 lines (Ctx, state keys, field readers, guards, route-group decls) and includes titlestate for the api TUs; **`update/scheduler` now gets the refresh driver without the http layer**.

Zero call-site changes: helpers keep their global-scope names; the api_ctx↔enrich include cycle stays as documented (benign, pragma-once).

## Verified
build clean, 22/22 tests, `api/detail|chapters`, `/view/*`, admin reload 200.

## Not done
- `str_field/num_field/kStateHost` stay in api_ctx (request shaping = http's job).
- `src/enrich` untouched (its own module per `decisions/enrich-module-location`).

generated: { by: "kit-agent", at: "2026-11-26" }

# Related Concepts
- [enrich module lives in src/enrich/, http/ = routes + shaping](enrich-module-location.md): extends the same layering rule to the title-state domain
- [Store: reuse one raw sqlite handle for api_cache (measured 0.4ms/call saved)](store-raw-handle-reuse.md): consumer of the moved refresh_title driver
