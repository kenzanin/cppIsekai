---
type: Decision
title: Plugin port to JS and WASM — plan
description: "Plan to port cppIsekai plugins to the JS (quickjs) and WASM (wasm3) runtimes from the goIsekai corpus, to expose each runtime's host-tool gaps explicitly."
tags: [plugin, port, js, wasm, goisekai]
generated: { by: agent/cli, at: "2026-11-12T08:19:15Z" }
status: draft
---

## Goal (user, 2026-11-12)

Port plugins to JS and WASM to find the gaps in each runtime. OpenSpec change to be created (spec-driven repo).

## Current state (verified 2026-11-12)

- plugins_dev/js/ and plugins_dev/wasm/ are EMPTY. No plugin has been ported to JS or WASM yet.
- plugins_dev/lua/ ships: atsumaru (184 loc, JSON API, Typesense, no crypto), mangadex (299 loc, auth + followedCount ranking), mangafire (241 loc + genres.lua, needs VRF — already ported to lua, uses host.crypto.vrf_sign), enrichment-mangadex (254), enrichment-mangaupdates (231).
- Host surface parity (from src/plugin):
  - Lua: host.http.fetch, host.url_encode/decode (host-only), host.now, host.log.info/error, host.fs.read, host.storage.get/set, host.config.get, host.crypto.vrf_sign, host.re2.match/find, host.html.select/attr, json.encode/decode, confined require() (module cache, merged siblings via fs.read+load not needed with require).
  - JS (quickjs, src/plugin/js/): host.now, host.log.info/error, host.http.fetch, host.fs.read, host.storage.get/set, host.re2.match/find, host.html.select/attr, require() (CommonJS, confined, cached on globalThis.__modules) and global JSON. MISSING vs lua: host.url_encode/decode, host.config.get, host.crypto.vrf_sign.
  - WASM (wasm3, src/plugin/wasm/, freestanding C): flat env imports only — now, log_info, fs_read, storage_get/set, http_fetch, re2_match/find, html_select/attr. No JSON parser, no string helpers, no crypto, no vrf_sign. Plugin must build the envelope JSON by hand in C and parse http_fetch's JSON reply itself (or via a bundled parser).
- No wasm toolchain verified yet: check for wasi-sdk / clang --target=wasm32 before starting komix (OKF references/goisekai-abi-mapping assumes wasi-sdk + C++17).
- tests: tests/test_plugin.cpp has jsfix/js_modtest/wasm fixtures; JS/WASM port tests can mirror those.

## Source corpus (NOT a dependency)

~/tmp/goIsekai (Go experiment, user's first): examples/plugins/{lua,js,wasm,native,yaegi}/<site>/. HAR captures in ~/tmp/goIsekai/tmp/*.har and tmp/har/. Port the LOGIC, never the Go source verbatim.

## Port verdict table (OKF references/goisekai-abi-mapping, checked 2026-11-12 — Lua plan, not JS/WASM)

| Site | Runtime plan | Blockers / notes |
|---|---|---|
| mangafire | lua (first) | ported already; needed host.crypto.vrf_sign |
| batcave | lua | host.http.fetch + host.re2 |
| weebcentral | lua | use host.html.select/attr RE2 |
| comik | lua | easiest |
| komix (komix.to) | wasm C++ from ~/tmp/goIsekai/examples/plugins/wasm/komix_to_cpp | rewrite wasm_abi.h host_call layer + RapidJSON; concrete imports: now, log_info, fs_read, storage_get/set, http_fetch, html_select, html_attr |
| mangapill | wasm | only Go tinygo source, full rewrite, lowest priority |

User has NOT chosen the JS/WASM targets yet. Candidate: atsumaru -> JS + WASM side-by-side (184 loc, JSON-only, no crypto) as the gap probe. comix/komix/mangapill per OKF table live under wasm.

## Open questions (user must decide before the OpenSpec change is authored)

1. Which plugins go to JS? (atsumaru? mangadex? none?)
2. Which plugins go to WASM? (komix per OKF? atsumaru probe? mangapill?)
3. Deliverable: working ports + gap doc in design.md, or OpenSpec plan only (proposal/design/tasks, no code), or ports only with gaps in commit message?
4. Host gap policy: add missing JS host tools (url_encode/decode, config.get, crypto.vrf_sign) as part of this program, or record gaps and leave them missing?
5. WASM plugin language: C (like tests/fixtures/plugins/wasm/fixture.c, no libc) or C++ + wasi-sdk (like komix_to_cpp)?

## Next step

OpenSpec change (spec-driven) once Q1-Q5 answered; use openspec new change + propose artifacts. OKF concept must be updated to stable with the chosen plan when decided.
