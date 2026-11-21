---
type: Finding
title: "Store::migrate_source inbound call sites"
description: "Single production caller (POST /api/migrate in src/http/http_api_library.cpp:198) plus five doctest call sites in tests/test_store.cpp."
tags: [store, library-migrate, call-graph, api]
generated: { by: agent/mcp, at: "2026-11-21T03:22:41Z" }
status: stable
---

Inbound callers of `Store::migrate_source` (src/store/store_migrate.cpp:16, declared src/store/store.hpp:316), traced 2026-11-21:

- src/http/http_api_library.cpp:198 - POST /api/migrate handler; the only production caller. Runs inside on_pool(db) after materialize_chapters, then recompute_dup_groups() on success.
- tests/test_store.cpp:974 - happy-path rekey + read-boundary carry (library-migrate TEST_CASE at :956).
- tests/test_store.cpp:1008,1010,1012,1013 - refusal cases: absent source, target already saved, degenerate self-migrate, empty old_plugin.

No other callers in src/ or tests/.

# Related Concepts
- [Library migrate-source: repoint, read-boundary carry](../decisions/library-migrate-source.md): Impact analysis of the migrate_source entry point recorded by the worker; relates to the migrate-source design decision.
