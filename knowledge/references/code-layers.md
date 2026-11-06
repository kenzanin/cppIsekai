---
type: reference
title: "Source layer map and rules (src/<scope>/, .mustache templates)"
description: "Layer map of the shipped tree: src grouped by scope (store/http/log), templates .mustache, Store is the only SQL, on_db is the only cross-thread path"
tags: [code, architecture, reference]
generated: { by: agent/mcp, at: "2026-11-06T14:06:18Z" }
status: stable
---

Layer map of the current tree (all next modules must slot in like this, not invent new call paths). src/ is grouped by scope; main.cpp stays at the root.

- src/main.cpp -- entrypoint: CLI11 flags (--port/--config), SimpleIni load, pools wiring (ui=crow x4, db=1, fetch=2 idle, log=1), logger setup, serve()
- src/store/ -- the ONLY SQL lives in Store (sqlite_orm behind Pimpl). Methods only: plugins/count/search. Seed 4 plugins x 12 titles in transaction_guard.
- src/http/ -- crow::SimpleApp. Routes: / (home.mustache), /library/grid (htmx partial swap of #grid from grid_body.mustache), /search /history /plugins /about (blank.mustache), /logs, /ws/log. All DB access via on_db() (exception-safe: set_exception + log + rethrow). Port resolved by caller and passed in.
- src/log/ -- QueueSink (spdlog base_sink) -> ReaderWriterQueue -> LogBuffer drains to data/cppisekai.log + 200-line ring; /ws/log replays snapshot.
- templates/*.mustache -- layout via partials (top.mustache/bottom.mustache); grid_body.mustache is the htmx fragment. NOTE: crow mustache partial names must include the extension ({{>top.mustache}}); set_base("templates"). Extension renamed .html -> .mustache for recognizability; page paths in http.cpp updated to match.
- tests/test_store.cpp -- doctest suite for Store; includes "store/store.hpp"; test_store.db gitignored.
- data/ -- runtime dir (library.db WAL + cppisekai.log), gitignored.

Rules going forward:
1. New module = new src/<scope>/ folder; main.cpp only wires it.
2. No SQL outside Store; no crow/log access outside the owning worker; cross-thread = post to that worker's pool, wait on future (on_db is the pattern, exception-safe).
3. Templates are .mustache; partial refs carry the extension.

# Related Concepts
- [Home demo: library grid, plugin filter, live log](../architecture/home-demo.md): Layer map shows how the home-demo decision is realized in source (paths updated after restructure)
