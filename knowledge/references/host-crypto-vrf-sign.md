---
type: Reference
title: "host.crypto.vrf_sign: host-executed VRF signing (tables plugin-side)"
description: "host.crypto.vrf_sign: mangafire-style VRF request signing host tool (Lua binding); tables plugin-side, host = transform only; vectors in tests/test_vrf.cpp."
tags: [plugin, crypto, vrf, abi, mangafire]
generated: { by: agent/mcp, at: "2026-11-12T05:39:35Z" }
status: stable
---

Added 2026-11-12 as part of openspec change `host-crypto`.

## Contract
`host.crypto.vrf_sign(api_path, params_table, stages) -> (sig, err)` in Lua plugins.
- sign string: `api_path` with leading literal `/api` trimmed; params serialized sorted by key as raw `k=v` joined with `&` (NO url-escaping — plugin passes already-raw values)
- stages `[{iv (0-255), key, tbl}]` declared by the plugin; key/tbl decoded from standard base64 (strict, local decoder — glaze read_base64 silently truncates on invalid input, not usable for error contract)
- transform: per stage `out[i] = tbl[(d ^ key[i % len(key)] ^ prev) & 0xFF]`, `prev` seeded to `iv`
- output: RawURL base64url (URL-safe alphabet, no padding)
- errors: `vrf_sign: stage <n>: ...` returned as `(nil, err)` pair like http.fetch's contract; never throws, never crashes the plugin call
- `tbl` must decode to at least 256 bytes (lookup table); `key` non-empty; `iv` integer 0-255
- Host ships ONLY the transform; site tables (VRF_K1..3/T1..3) stay in the mangafire plugin file. Host has no knowledge of endpoint names. No caching/memoizing.

## Implementation
- `src/plugin/vrf.hpp` — pure header-only transform (shared seam; JS/wasm bindings can reuse later)
- Lua binding in `src/plugin/lua/lua_runtime.cpp` under host.crypto; params coerced: strings passthrough, scalars (int/double/bool) via tostring/`"true"|"false"`, nested tables rejected
- Note: `std::to_string(double)` yields e.g. `50.000000`; plugins should pass params as strings (mangafire does)
- Tests: `tests/test_vrf.cpp` (new target `test_vrf` in xmake.lua + CMakeLists.txt) — golden vectors ported byte-for-byte from goIsekai vrf_sign_test.go (empty/one/sorted/apiStrip/utf8) + error cases; `tests/fixtures/plugins/crypto/` inline-plugin path check via test_plugin.cpp

## Status
Lua binding shipped; JS/wasm bindings deferred (not needed yet). Mangafire plugin port (using this tool) is the next work item — see `references/goisekai-abi-mapping`.
