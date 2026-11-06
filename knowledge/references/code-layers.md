---
type: reference
title: Source layer map and rules (store / log / http)
description: "Layer map of the shipped tree: Store is the only SQL, QueueSink is the whole log bridge, on_db() is the only cross-thread path; rules for future modules"
tags: [code, architecture, reference]
generated: { by: agent/mcp, at: "2026-11-06T11:49:37Z" }
status: stable
---

Layer map of the current tree (all next modules must slot in like this, not invent new call paths)

File/layer:
- src/store.hpp / store.cpp -- the ONLY SQL in the program lives in Store. One sqlite connection, methods only (plugins, count(plugin,q), search(plugin,q,limit,offset)), every statement prepared, LIKE input escaped with ESCAPE '\\'. Seed 4 plugins x 12 titles on first run when table is empty.
- src/log.hpp -- QueueSink (spdlog::sinks::base_sink) pushes every formatted line into moodycamel::ReaderWriterQueue g_log_queue; LogBuffer drains it on the log worker to data/cppisekai.log + 200-line ring; /logs page renders snapshot, /ws/log websocket replays it on connect (node client test got 2 lines). Default logger = stdout_color logger with the QueueSink appended, so every spdlog::info lands there.
- src/http.cpp -- crow::SimpleApp. Routes: / (home mustache), /library/grid (htmx partial swap of #grid), /search /history /plugins /about (blank.html placeholders), /logs, /ws/log. All DB access goes through on_db(): post a lambda to the 1-thread db pool, block on future. htmx 4.0.1 + alpine 3.17.5 vendored in static/, Tailwind via CDN.
- src/main.cpp -- pools: ui (crow concurrency 4), db (1), fetch (2, idle), log (1). Store + LogBuffer constructed here, serve()
- templates/top.html|bottom.html -- layout partials, every page factors them; grid_body.html is the htmx-swappable fragment.
- data/ -- runtime dir, library.db (WAL) + cppisekai.log, does not go to git.

Rules going forward:
1. No SQL outside Store; route handlers never compose SQL strings.
2. No crow/websocket/log/table access outside the owning worker; cross-thread = post to that worker's pool, wait on the future.
3. Every page is quot;shared layout partial + a page contained content form layout

# Related Concepts
- [Home demo: library grid, plugin filter, live log](../architecture/home-demo.md): Code layer map shows how the home-demo decision is realized in source
- [cppIsekai dependency API surface](dependency-api.md): Log bridge uses the verified ReaderWriterQueue/spdlog entry points
