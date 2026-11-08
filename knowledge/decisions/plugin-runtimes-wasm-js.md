---
type: decision
title: "Plugin runtimes: wasm3 + quickjs-ng execution contract"
description: "Change plugin-js-wasm 9/9: dual runtime (wasm3 ABI + quickjs realm) behind ONE shared shape/health core; parity host tools; confined fs.read; gas/interrupt budgets; hardening lessons (JS refcount consume, per-invoke stack rebase, glz brace-init)."
tags: [plugin-runtime, wasm, quickjs, abi, sandbox, security]
generated: { by: agent/mcp, at: "2026-11-08T06:00:00Z" }
status: stable
---

Kontrak ini mengeksekusi rencana runtime-parity dan vendoring dari `decisions/deps-and-plugins`, dan memperluas ABI di `architecture/plugin-system`. Dieksekusi lewat change `plugin-js-wasm` (9/9 task; independent review PREAPPROVED 0C/0I/4m; ctest 10/10; live smoke HTTP js+wasm).

## Contract

Envelope plugin tetap `{api,method,params}` JSON-in, `{ok,data}`/`{ok,error}`-out. **Satu** shape-gate (`plugin::checked_reply`) dan **satu** core health/call-mutex di `Host::call`; per-runtime seam hanya marshal. Runtime dari manifest; gagal load = plugin absent + warn.

## WASM (wasm3, vendored)

Module exports `alloc(i32)->ptr`, `handle(ptr,len)->ptr (0=fail)`, `last_len()->i32` di atas linear memory; imports `env.{now,log,fs.read,storage.get/set,http.fetch}` via `m3_LinkRawFunctionEx` + userdata instance; **tanpa WASI**. Budget = gas `m3_SetResourceLimit` (armed pre-compile, refresh per call); trap apa pun => ok:false. Host selalu bounds-check pointer guest (`guest_mem`).

## JS (quickjs-ng d4187c7, core only)

Entry = source yang di-eval di realm per-plugin; entry mendefinisikan global `handle(env)`. Globals host: `now`, `log.info/error`, `http.fetch`, `fs.read`, `storage.get/set` (fs via `plugin::fs_read` terkonfini di folder plugin; http memakai jalur curl-sync yang sama dengan lua). Budget = interrupt handler 200k ops (chunk 512), **di-init sebelum eval entry**.

## Lessons (hard-won)

1. `JS_SetPropertyStr` **mengonsumsi** ref value -- jangan free setelahnya.
2. Stack base thread pool beda dari thread loader: `JS_UpdateStackTop(rt)` di **setiap** invoke, else "Maximum call stack size exceeded" untuk parse sekecil apa pun.
3. `glz::generic{T}` brace-init merusak skalar -- assign via `.data`.
4. Shape-check harus runtime-agnostic dan di shared core.

## Parity & keamanan

Host tools sama nama/semantik lintas-runtime; deviasi tercatat: http-gagal lua=nil+msg vs js=null (ponytail). `fs.read` read-only, konfine leksikal (prefix+boundary) ke folder plugin; symlink escape ditoleransi v1.

## Fixtures & toolchain

`tests/fixtures/plugins`: wasm echo/bad/spin-gas freestanding `clang --target=wasm32` (wasi-sdk ada di `/opt/wasi-sdk`, sengaja belum dipakai -- steer user), js fix/bad; perintah regen di README tiap dir.

## Deferred (accepted by review)

Import-smoke sisi guest; test bad-reply-shape wasm; pruning global realm js; penyatuan shape http-fail; penamaan `make_wasm_fetch`; granularitas gas vs op-hook lua; warning `test_plugin:137` pre-existing.

## Related

- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](deps-and-plugins.md): Ruling parity + vendoring yang kontrak ini eksekusi.
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): ABI data-contract yang layer runtime ini perluas dengan jalur eksekusi js/wasm.
