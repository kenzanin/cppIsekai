---
type: decision
title: "Dependency vendoring: system-shared first, minimal vendoring"
description: Third-party dependency surface after xmake xrepo migration round 2
tags: [dependencies, xrepo, vendoring, lua, utf8proc, re2]
generated: { by: agent/mcp, at: "2026-11-13T00:40:08Z" }
status: stable
---

Dependency policy (updated 2026-11-13, xmake migration complete): system packages via pacman/pkg-config — libvips 8.18.x, lua 5.4, curl, crow 1.3.x (bundles asio), cli 11 — everything else pins via xrepo in xmake.lua, locked in xmake-requires.lock (committed):

- glaze 7.9.0 (JSON), sol2 3.5.0 (Lua binding), thread-pool 5.1.0 (BS::thread_pool), readerwriterqueue 1.0.7, lexbor 3.0.0 (HTML), libzippp 7.1 (CBZ; header nested as libzippp/libzippp.h), quickjs-ng 0.15.1 (JS runtime; JS_SetInterruptHandler takes runtime not ctx), simpleini 4.27 (config), sqlite_orm 1.9.1 (store).

Vendored remainder under third_party/ (no xrepo equivalent): wasm3 and curl-impersonate only; include dirs wired from xmake.lua target cfg.

History: before 2026-11-13 the nine packages above were vendored header/source copies under third_party/; removed in the xmake migration (see git log, build: subjects). System-package versions evolve via pacman; the pin table is the compatibility contract.
