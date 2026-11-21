---
type: Finding
title: "Store::migrate_source inbound call sites (graph smoke review)"
description: "One production caller (POST /api/migrate, src/http/http_api_library.cpp:198) plus five doctest call sites in tests/test_store.cpp; the symbol is absent from the codebase-memory graph, so search_graph/trace_path could not resolve it and the route was confirmed from source."
tags: [store, migrate, call-graph, index-gap]
generated: { by: agent/mcp, at: "2026-11-21T03:31:26Z" }
status: stable
---

# Store::migrate_source inbound call sites

Requested via `search_graph` (inbound callers) then `trace_path` (route confirmation).

## Tooling outcome

- `search_graph(name_pattern=".*migrate.*", project=cppIsekai)` returned exactly one
  match: `migrate_saved_manga` (`src/store/store_core.cpp:24-84`). `migrate_source`
  was **not** in the result set.
- `trace_path(function_name="migrate_source", direction=inbound)` failed with
  `function not found`.
- `search_code(pattern="migrate_source")` found the declaration and definition in
  source but no graph node, i.e. the index is stale for `src/store/store_migrate.cpp`.

Conclusion: the call graph could not answer the query. Route was confirmed by
reading source directly (no source edits made).

## Call sites

Production (1):

- `src/http/http_api_library.cpp:198` — inside the `POST /api/migrate` handler
  (`CROW_ROUTE(app, "/api/migrate")`, line 133), executed within
  `on_pool(db, ...)` at line 196. On success it calls
  `store.recompute_dup_groups()` at line 201.
  Chain: `POST /api/migrate` → `on_pool(db)` → `Store::migrate_source`.

Tests (5, all in `tests/test_store.cpp`, case
`"Store: migrate_source rekeys the entry and carries the read boundary (library-migrate)"`
at line 956):

- `:974` — happy path (`ok` asserted)
- `:1008` — unknown source plugin → false
- `:1010` — source not in library → false
- `:1012` — identical from/to pair → false
- `:1013` — empty source plugin → false

Definition: `src/store/store_migrate.cpp:16` (`bool Store::migrate_source(...)`,
`OpTrace _op{"migrate_source"}` at line 19). Declaration:
`src/store/store.hpp:316`.

## Follow-up

Run `index_repository` for `cppIsekai` so `Store::migrate_source` enters the graph;
until then graph-based impact analysis on this symbol is unreliable.

## Related

- `findings/store-migrate-source-callers` covers the same call sites in more detail.
- [Store::migrate_source inbound call sites](store-migrate-source-callers.md): Same call-site inventory, more detail; this finding records the graph-index gap that blocked the tooling path.
