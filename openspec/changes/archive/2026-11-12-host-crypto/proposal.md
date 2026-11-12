# Proposal

## Why

Porting the goIsekai mangafire plugin (approved by user 2026-11-12) requires the host to sign API requests with the site's VRF transform — every upstream endpoint answers 403 without it. cppIsekai has no crypto host tool today; the earlier `no crypto yet` YAGNI note (OKF `decisions/host-url-codec`) explicitly deferred this until a plugin needed it, and mangafire is that plugin.

## What Changes

- New host tool `crypto.vrf_sign(api_path, params, stages)` exposed to plugins under `host.crypto` (Lua binding first; the C++ algorithm lives behind a shared seam so JS/wasm bindings can reuse it later).
- VRF algorithm ported byte-for-byte from the goIsekai implementation (`internal/pluginutil/vrf_sign.go`): stages `[{iv, key, tbl}]` supplied by the plugin as data (key rotation = plugin-side constant edit, no host rebuild), base64 std decode of key/tbl, three chained XOR-table passes, RawURL base64url signature.
- Plugin-side (not host): the mangafire VRF tables stay in the plugin file (site secret) — the host only executes the transform.
- Site-specific request signing semantics that are mangafire-only stay in the plugin (empty `keyword` omitted entirely; array params signed in indexed form `genres_in[0]=13` but sent as `genres_in[]=13`); the host tool is a generic transform.
- Unit tests in the existing `tests/` doctest suite (signing against vectors adapted from goIsekai's `vrf_sign_test.go`).

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `plugin-host-tools`: adds a crypto host-tool requirement (`crypto.vrf_sign`) to the contract every runtime binding must implement identically, plus the shape the result takes and the security boundary (plugin supplies tables, host supplies only the transform).

## Impact

- `src/plugin/` — new crypto TU(s) with the VRF transform + base64 (glaze `third_party/glaze/glaze/base64`), wired into the Lua host tools (`src/plugin/lua/lua_runtime.cpp`) under `host.crypto`.
- `tests/` — new doctest target (or extended `test_plugin`) covering the signing vectors.
- `openspec/specs/plugin-host-tools/spec.md` — delta spec adding the new requirement.
- GoIsekai HAR captures remain the behavioral reference; no Go code is copied verbatim.
- No changes to fetch stack, store, config, or existing host tools; no new dependencies.
