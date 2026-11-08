# Design

## Context

`src/plugin/plugin.cpp` today: sol2/Lua only. `Runtime` holds a `lua_State*`; `Host::call`
builds the envelope, resolves `handle`, validates the response shape, and keeps health
bookkeeping — all inline with Lua types. wasm3 is vendored and linked (`third_party/wasm3`,
static lib target) with zero call sites. quickjs-ng is absent. Host tools (`http.fetch`,
`storage.*`, `log.*`, `now`) are installed as Lua globals per plugin; the Lua `io` shim is
ad-hoc (`io.read` = stdin, `io.lines(path)` = unchecked ifstream). Manifest `runtime` is
already parsed and stored (`Runtime::info.runtime`) but never dispatched on. See
proposal.md — Why.

## Goals / Non-Goals

**Goals:**
- One dispatch point: `Host::call` branches on manifest `runtime`; Lua path behavior
  unchanged (byte-identical responses for existing plugins).
- js + wasm reach feature parity on the host-tool surface, including a confined
  read-only `fs.read`, in one change.
- Budget/hang protection parity with the Lua instruction hook.

**Non-Goals:**
- micropython runtime; plugin write access; WASI sdk; hot reload; changing the
  `call_cached` wrap point or any HTTP route shape.

## Decisions

- **D1 — Runtime variant, not inheritance.** `Runtime` becomes
  `std::variant<LuaRt, WasmRt, JsRt>` (or three optional members); `Host::call` keeps the
  envelope/health/shape-validation core runtime-agnostic and delegates only
  "invoke entry with envelope, get JSON out" to the runtime adapter. Rationale: the
  response-shape and health rules are exactly what must not drift per runtime.
- **D2 — Adapter boundary = JSON-in/JSON-out.** Each adapter owns only: load entry,
  marshal `{api,method,params}` in, marshal response object out, map runtime exceptions
  to `ok:false`. Lua keeps sol2 tables internally; js gets a plain object; wasm gets
  UTF-8 JSON in linear memory.
- **D3 — wasm entry contract.** Module exports `handle(in_ptr:i32, in_len:i32) -> i32`
  (0 = failure) plus `alloc(i32) -> i32` and `result_len() -> i32`; host writes the
  envelope at `alloc`ed memory, reads the result pointer/len back. Imports (host tools)
  are plain `(ptr,len)->i64`-style functions registered per instance; **no WASI** — the
  module sees nothing but host imports. Fuel + stack limits from wasm3 exec config.
- **D4 — js realm contract.** Per-plugin `JSRuntime`+`JSContext`; globals whitelisted to
  the host-tool object (`http`, `storage`, `log`, `fs`, `now`, `JSON`, `Math`, `parseInt`
  family); entry = global `handle(env)`; budget via `JS_SetInterruptHandler` counter
  checked against `plugin_timeout_ops`-derived op count.
- **D5 — fs.read core, one implementation.** A single C++ helper
  (`plugin::fs_read(plugin_dir, rel) -> optional<string>`): lexically normalizes
  `rel`, rejects absolute/`..`-escaping paths, reads regular files only. Bound as
  `fs.read` in all three runtimes; Lua `io.read`/`io.lines` remain untouched for compat.
- **D6 — quickjs-ng vendored static.** Sources under `third_party/quickjs/` (upstream
  release snapshot + LICENSE/attribution header), compiled into the `cppIsekai` target
  like wasm3; no pkg-config. Zero external deps beyond libc.
- **D7 — fixtures prebuilt.** `tests/fixtures/plugins/{js,wasm}/` echo plugins; the
  `.wasm` is committed prebuilt (clang `--target=wasm32` needed only to regenerate; a
  `tests/fixtures/README.md` documents the one-liner). ctest stays hermetic: no network,
  tmp plugin roots, host tools stubbed where meaningful (storage via a scratch dir
  backend as the existing tests do).

## Risks / Trade-offs

- wasm ABI (D3) is a host-defined convention; wrong pointer/len handling segfaults the
  host — mitigated by the trivial fixture and bounds-checked copies on both sides.
- quickjs snapshot pinning: record upstream commit in `third_party/quickjs/VERSION`
  note; upgrades are manual, same policy as other vendored deps.
- Interrupt-budget granularity differs per runtime (ops vs fuel): accepted; the contract
  is "bounded and ok:false", not identical counters.

## Open Questions

None blocking; wasm fixture regeneration toolchain availability is checked at apply
time (regeneration only, never needed for ctest).
