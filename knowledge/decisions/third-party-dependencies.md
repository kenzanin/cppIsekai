---
type: decision
title: "Dependency vendoring: system-shared first, minimal vendoring"
description: Third-party dependency surface after xrepo pinning of crow/cli11/asio/lua (2026-11-15 round 3); system shared deps reduced to libvips + curl
tags: [dependencies, xrepo, vendoring, lua, utf8proc, re2]
generated: { by: agent/mcp, at: "2026-11-15T14:51:22Z" }
status: stable
---

Dependency policy (updated 2026-11-15, round 3): system packages via pacman/pkg-config — ONLY libvips 8.18.x (pkgconfig::vips) and curl (system .so for dlopen fetch paths). Everything else pins via xrepo in xmake.lua, locked in xmake-requires.lock (committed):

- glaze 9.1.0 (JSON; 7.9.0 does not compile under GCC16), sol2 3.5.0 (Lua binding, configs={includes_lua=false}), lua 5.4.8 (xrepo pin — xrepo default resolves to 5.5 which sol2 3.5.0's compat header rejects), thread-pool 5.1.0, readerwriterqueue 1.0.7, lexbor 3.0.0, libzippp 7.1, quickjs-ng 0.15.1, simpleini 4.27, sqlite_orm 1.9.1, sqlite3 3.54.0, utf8proc 2.11.3, re2 2025.11.05, date 3.0.4, hash-library 2021.09.29, pugixml 1.16, spdlog 1.17.0, zlib 1.3.2, backward-cpp. Host HTTP: crow 1.3.5 (ssl=false — daemon is plain HTTP; zlib compression kept for CROW_ENABLE_COMPRESSION) + asio 1.36.0 (crow transitive, pinned) + cli11 2.7.2 (configs={cmake=false}, headeronly).

Crow/cli11/asio/lua were migrated from system pacman to xrepo pins on 2026-11-15 (both are header-only, so they were already compiled into the binary; the change pins versions in the lock instead of letting pacman upgrades silently change the build, and drops the pacman prereqs).

Vendored remainder under third_party/ (no xrepo equivalent): wasm3 and curl-impersonate only.

History: before 2026-11-13 the nine packages above were vendored header/source copies under third_party/; removed in the xmake migration (see git log, build: subjects).
