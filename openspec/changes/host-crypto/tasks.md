# Tasks

## 1. VRF transform (pure C++)

- [ ] 1.1 Add `src/plugin/vrf.hpp`: `vrf_sign(api_path, params_map, stages) -> std::optional<std::string>` implementing signStr (leading `/api` trimmed, sorted raw `k=v` pairs, no URL-escaping), 3-stage XOR `out[i] = tbl[(d ^ key[i % len] ^ prev) & 0xFF]` with `prev` seeded to `iv`, RawURL base64url output (local ~15-line encoder, URL-safe alphabet, no padding) with glaze base64 for `key`/`tbl` decode; bad input returns `nullopt` with a `vrf_sign: stage <n>: ...` reason — verify: build passes zero-warning.
- [ ] 1.2 Unit vectors: port goIsekai `vrf_sign_test.go` cases (fixed table set, deterministic output, empty params, multi-stage order, bad base64 in key/tbl, out-of-range `iv`) into `tests/test_vrf.cpp` as a new doctest target; register the target in xmake.lua and CMakeLists.txt alongside the existing test targets — verify: vector cases pass, new target runs under `xmake test`.

## 2. Lua binding

- [ ] 2.1 Bind `crypto.vrf_sign(api_path, params_table, stages) -> (result, err)` in `src/plugin/lua/lua_runtime.cpp` under `host.crypto` (same shape as `host.http.fetch`'s pair): params coerced to string map (scalars via tostring, nested tables rejected), `iv` integer 0-255, decode errors reported with stage index — verify: `test_plugin` still passes; a 5-line inline plugin snippet called through a doctest harness returns expected signature.
- [ ] 2.2 Verify the full host loads + the Lua sandbox exposes `host.crypto` and existing plugins (mangadex, atsumaru) still load and pass a smoke call — verify: `xmake test` full suite passes; manual or scripted call to a known vector via the plugin path matches.

## 3. Documentation + close-out

- [ ] 3.1 Update `plugin-host-tools` main spec via openspec sync (once archive happens) or record the host tool in OKF: crypto.vrf_sign (tables plugin-side, host = transform only) — verify: OKF `okf_search(query="crypto")` returns the new note alongside `host-url-codec`.
