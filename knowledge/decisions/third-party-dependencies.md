---
type: decision
title: "Dependency vendoring: system-shared first, minimal vendoring"
description: "Dependencies: crow/asio/libvips/lua54/curl-headers from pacman; sol2, BS::thread_pool, readerwriterqueue, glaze, wasm3, libsql amalgamation and curl-impersonate vendored minimal; pin hashes in README"
generated: { by: agent/mcp, at: "2026-11-06T08:11:58Z" }
status: stable
---

Source: tmp/konsep_awal.md. System packages (pacman / pkg-config): libvips 8.18.7, lua54 5.4.9, curl headers 8.23, crow 1.3.4, asio 1.38.2 (under Crow). Vendored under third_party/ (headers/source + LICENSE only): sol2 v3.5.0 9190880 (MIT, include <sol/sol.hpp>), BS::thread_pool bd4533f (MIT), readerwriterqueue 6867b56 (BSD), glaze 0a363d4 (MIT), wasm3 28ecb9a sources (MIT, static), libsql sqlite amalgamation 3.47.0 from libsql-ffi/bundled/src (PD, static), curl-impersonate v0.6.1 prebuilt libcurl-impersonate-chrome.so.4.8.0 (shared, BUILD_RPATH). The main.cpp smoke binary proves link+runtime for all. Vendored total ~38M. Details and licenses: README.md.

# Related Concepts
- [Build system: CMake + C++23, exe-only](build-toolchain-cpp23.md): Dependency vendoring decision executes the toolchain decision
