---
type: decision
title: Guest runtime language policy
description: "cppIsekai wasm guests are C++ (wasi-sdk libc++ on disk, clang++); the mangafreak guest ported C→C++ 2026-11-15; Odin not needed."
tags: [wasm, guest, language, decision, plugin]
generated: { by: agent/mcp, at: "2026-11-15T15:47:07Z" }
status: stable
---

## Context
mangafreak wasm guest (freestanding C) bugfix 2026-11-14: uninit dedup `seen[][]` caused false-dup result dropping; cover fallback read stale scratch buffer. Both fixed (456102d). Static-buffer aliasing remained the cost center of hand-C guests.

## Decision
- C++ via wasi-sdk is the default guest language: clang++ + sysroot libc++/libc++abi already on disk, zero new deps.
- **mangafreak guest ported main.c → main.cpp 2026-11-15** (commit 6e589c0): rename + `extern "C"` on alloc/handle/last_len (wasm-ld --export looks up the undecorated symbol; C++ mangles — without it the module builds 283 B with no CODE section and silently registers 0 functions), six void*→const char* casts (memmem/memchr), `_GNU_SOURCE` drop (sysroot defines it).
- Odin (/sbin/odin) remains viable only if a guest outgrows C++; xmake toolchain rule needed, not worth it yet.

## Revisit
When the third wasm guest lands, default it to C++. Odin only with xmake toolchain buy-in.
