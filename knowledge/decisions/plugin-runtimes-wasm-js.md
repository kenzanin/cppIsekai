---
type: decision
title: "Plugin runtimes: wasm3 + quickjs-ng execution contract"
description: "Change plugin-js-wasm 9/9: dual runtime (wasm3 ABI + quickjs realm) behind ONE shared shape/health core; parity host tools; confined fs.read; gas/interrupt budgets; hardening lessons (JS refcount consume, per-invoke stack rebase, glz brace-init)."
tags: [plugin-runtime, wasm, quickjs, abi, sandbox, security]
generated: { by: agent/mcp, at: "2026-11-08T06:00:00Z" }
status: stable
---

This contract executes the runtime-parity and vendoring plan from
`decisions/deps-and-plugins`, and extends the ABI in `architecture/plugin-system`.
Delivered via change `plugin-js-wasm` (9/9 tasks; independent review PREAPPROVED
0C/0I/4m; ctest 10/10; live HTTP smoke js+wasm).

## Contract

The plugin envelope stays `{api,method,params}` JSON-in, `{ok,data}`/`{ok,error}`-out.
**One** shape gate (`plugin::checked_reply`) and **one** health/call-mutex core in
`Host::call`; per-runtime seams only marshal. Runtime from the manifest; load failure =
plugin absent + warn.

## WASM (wasm3, vendored)

Module exports `alloc(i32)->ptr`, `handle(ptr,len)->ptr (0=fail)`, `last_len()->i32` over
linear memory; imports `env.{now,log,fs.read,storage.get/set,http.fetch}` via
`m3_LinkRawFunctionEx` + instance userdata; **no WASI**. Budget = gas
`m3_SetResourceLimit` (armed pre-compile, refreshed per call); any trap => ok:false. The
host always bounds-checks guest pointers (`guest_mem`).

## JS (quickjs-ng d4187c7, core only)

Entry = source evaluated in a per-plugin realm; the entry defines a global `handle(env)`.
Host globals: `now`, `log.info/error`, `http.fetch`, `fs.read`, `storage.get/set` (fs via
`plugin::fs_read` confined to the plugin folder; http reuses the lua curl-sync path).
Budget = interrupt handler, 200k ops (512/call chunks), **initialised before the entry
eval**.

## Lessons (hard-won)

1. `JS_SetPropertyStr` **consumes** the value ref - never free it afterwards.
2. The pool thread's stack base differs from the loader thread's:
   `JS_UpdateStackTop(rt)` on **every** invoke, else "Maximum call stack size exceeded"
   on any parse.
3. `glz::generic{T}` brace-init mangles scalars - assign via `.data`.
4. The shape check must be runtime-agnostic and live in the shared core.

## Parity & security

Host tools keep the same names/semantics across runtimes; recorded deviation: http
failure lua=nil+msg vs js=null (ponytail). `fs.read` is read-only, lexically confined
(prefix+boundary) to the plugin folder; symlink escape tolerated in v1.

## Fixtures & toolchain

`tests/fixtures/plugins`: freestanding `clang --target=wasm32` wasm echo/bad/spin-gas
(wasi-sdk present at `/opt/wasi-sdk`, deliberately unused - user steer); js fix/bad
pairs; regen commands in each README.

## Deferred (accepted by review)

Guest-side import smoke; the wasm bad-reply-shape test; js realm global pruning; http
failure-shape unification; `make_wasm_fetch` naming; gas vs op-hook granularity; the
pre-existing `test_plugin:137` empty-body warning.

## Related

- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](deps-and-plugins.md): The parity + vendoring ruling this contract executes.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): The data-contract ABI this runtime layer extends with js/wasm execution.
