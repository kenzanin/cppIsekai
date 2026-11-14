---
type: decision
title: Guest runtime language policy
description: cppIsekai wasm guests stay hand-C for regex-scrape plugins; C++ (wasi libc++) is the path for the next nontrivial guest; Odin only with xmake toolchain buy-in.
tags: [wasm, guest, language, decision, plugin]
generated: { by: agent/mcp, at: "2026-11-14T01:18:32Z" }
status: stable
---

## Context
mangafreak wasm guest (freestanding C) bugfix 2026-11-14: uninit dedup `seen[][]` caused false-dup result dropping; cover fallback read stale scratch buffer. Both fixed (456102d). Static-buffer aliasing remains the cost center of hand-C guests.

## Decision
- Keep C for existing guests; no rewrite of working guests.
- Next guest needing real data structures: C++ via wasi-sdk (libc++.a in sysroot, same clang, zero new deps).
- Odin (/sbin/odin, core:encoding/json) viable but needs xmake toolchain rule; only if a guest outgrows C++.

## Revisit
When a guest needs JSON-heavy parsing or the third wasm guest lands.
