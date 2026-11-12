# Tasks

## 1. url_encode/url_decode in JS and WASM

- [x] 1.1 Add `src/plugin/url_codec.hpp` (header-only): `url_encode(s)` — unreserved `A-Za-z0-9-_.~` pass through, remaining bytes as uppercase `%XX` (UTF-8 byte-wise); `url_decode(s)` — strict `%XX` pairs, everything else verbatim, `+` is NOT decoded to space (goIsekai queryEncode semantics). Verify: zero-warning build.
- [x] 1.2 Lua binding: audit the existing `host.url_encode/url_decode` in `src/plugin/lua/lua_runtime.cpp` — point it at the same core if semantics diverge; if it already matches, leave it and only add the new runtimes. Verify: `test_plugin` green after the refactor (or noted as no-op).
- [x] 1.3 JS binding: `host.url_encode(s)` / `host.url_decode(s)` in `src/plugin/js/js_bindings.cpp` (Int-out-of-scope, strings only, one arg, TypeError otherwise). Verify: `test_plugin` JS cases cover round-trip + `a b&c=d%?`.
- [x] 1.4 WASM imports: flat `url_encode`/`url_decode` in `src/plugin/wasm/wasm_imports.cpp` (+ minimal `wasm_internal.hpp` plumbing if the import table needs it). Verify: WASM fixture module calls both imports and echoes the round-trip; `test_plugin` green.

## 2. config.get in JS (parity with Lua)

- [x] 2.1 `host.config.get(key)` in `src/plugin/js/js_bindings.cpp`: read `config.json` in the plugin's own folder via `plugin::fs_read`, parse with glaze (or existing lenient parse), missing file/key → nil. WASM out of scope (documented). Verify: `test_plugin` JS case — present key returns value, absent key returns nil, missing file returns nil (no crash).

## 3. crypto.vrf_sign in JS (parity with Lua)

- [x] 3.1 `host.crypto.vrf_sign(api_path, params, stages)` in `src/plugin/js/js_bindings.cpp`, binding the shared `src/plugin/vrf.hpp` core. Params object: string values only (non-string / nested → error path); stages array `{iv, key (std-base64), tbl (std-base64)}`. Failure → `null` (Lua nil parity), reason logged tagged with plugin id. Verify: `test_plugin` JS case — signature for the known VRF vector matches `tests/test_vrf.cpp`; invalid stage → nil.

## 4. http.fetch opts parity audit

- [ ] 4.1 Audit `make_wasm_fetch` + JS binding + Lua front for opts `{timeout_ms, profile, method, body, accept, user_agent, headers, cookie}`: (a) keys identical across runtimes, (b) JS failed fetch → `null`, (c) reply `{status, body, headers}` parses in all three. Fix only real divergences inside this change; record the audit result in the apply session. Verify: `test_plugin` fetch/boundary cases still green; any fix comes with a test.

## 5. Integration check

- [ ] 5.1 Full suite green (`xmake test`), zero build warnings, daemon boots on a scratch db; `test_plugin` count increases by the new JS/WASM cases. Verify: build + test + scratch boot log shows scheduler line as before.
