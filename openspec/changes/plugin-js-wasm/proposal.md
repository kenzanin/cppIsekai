# Proposal

## Why

The host links **wasm3** (static, `CMakeLists.txt`) but never executes it — `src/plugin/plugin.cpp` dispatches every plugin through the sol2/Lua path only, so `runtime: "wasm"` manifests are dead weight. The **JS runtime has no implementation at all**: quickjs-ng was locked as the second runtime in `knowledge/decisions/deps-and-plugins.md` (user-confirmed 2026-11-07: runtime parity, same signature) but was never vendored. Separately, the user ruled plugins must be able to **read files from their own plugin folder** (e.g. `config.json`, dynamic data): Lua has a read-only `io` shim, but wasm/js have no path, and even Lua's shim is cin/ifstream ad-hoc rather than a scoped, plugin-dir-confined capability.

## What Changes

- **JS runtime (new)**: vendor **quickjs-ng** as a static C library under `third_party/quickjs/`; plugins with `runtime: "js"` execute `handle(env)` — JSON-shaped object in (mirroring the Lua envelope `{api, method, params}`), result object/`{ok,error}` out; host serializes at the boundary. Sandbox: fresh realm per plugin, whitelisted globals only, interrupt-based instruction budget.
- **WASM runtime (wire-up)**: implement the missing call path on the already-linked **wasm3**: entry = module export `handle` operating on linear memory (JSON UTF-8 in/out via documented `alloc`/`len` exports); host imports provide the same host tools; no WASI filesystem — everything goes through host imports; fuel/stack limits from `plugin_timeout_ops`-family config.
- **Shared runtime layer (refactor)**: extract the envelope build / response-shape validation / health bookkeeping out of the Lua-only `Host::call` into a runtime-agnostic core; `Runtime` becomes a variant (lua state | wasm instance | js realm) dispatched by manifest `runtime`.
- **Plugin-dir file read (all runtimes)**: new host tool `fs.read(rel_path)` — read-only, path resolved and **confined to the plugin's own folder** (no `..` escape, no absolutes), returns file contents or `null`. Bound identically in lua/wasm/js. The existing Lua `io.read`/`io.lines` shim stays for backward compat.
- **Fixtures + tests**: one minimal echo plugin per runtime under `tests/fixtures/` (wasm fixture committed prebuilt; regenerating needs clang wasm32 target), ctest targets per the existing one-target-per-module convention folded into `test_plugin`.

Out of scope: micropython runtime, plugin *write* access, WASI sdk dependency, hot-reload.

## Capabilities

### New Capabilities

(none — runtime behavior extends the existing plugin contract)

### Modified Capabilities

- `plugin-system`: adds requirements for non-Lua runtime execution (wasm call ABI, js realm contract), the shared JSON envelope parity rule, and the confined read-only `fs.read` host tool.

## Impact

- **Files**: `src/plugin/plugin.{hpp,cpp}` (runtime variant + dispatch + fs tool), `src/plugin/wasm_rt.{hpp,cpp}`, `src/plugin/js_rt.{hpp,cpp}`, `third_party/quickjs/**` (vendored), `CMakeLists.txt` (quickjs static lib + new sources), `tests/test_plugin.cpp` (+ fixtures), `plugins_dev/` examples.
- **New dependency**: quickjs-ng (vendored, static, no external deps) — per `decisions/third-party-dependencies` minimal-vendoring rule; wasm3 already present.
- **Behavior**: plugins with `runtime: "js"`/`"wasm"` go from ignored/unexecutable to first-class; Lua plugins unchanged; `GET /plugin/<id>/<method>` and all existing routes keep working (single `call_cached` wrap point untouched).
- **Risk**: wasm fixture toolchain (clang `--target=wasm32`) needed only to regenerate fixtures, not to build; quickjs vendoring size ~1 MB sources.
