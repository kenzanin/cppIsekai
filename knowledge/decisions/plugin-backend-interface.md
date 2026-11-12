---
type: Decision
title: Plugin backends stay struct+dispatch; introduce PluginBackend interface only for a 4th runtime or out-of-process
description: "Plugin backends stay a struct with concurrent runtime members plus dispatch in Host::call; introduce a polymorphic PluginBackend only when adding a 4th runtime or going out-of-process."
tags: [plugin, runtime, abi, architecture, interface]
generated: { by: agent/mcp, at: "2026-11-12T06:27:44Z" }
status: stable
---

User-confirmed 2026-11-12: the multi-language plugin path is NOT gated by a per-language class hierarchy today, and that is intentional.

## Current shape
- `plugin::Host` is the single public door: `call` / `enrich` / `load_all`; envelope rules, health bookkeeping, per-Runtime mutex, proxy, cookie handling all live there.
- `Runtime` (src/plugin/plugin_internal.hpp) is a plain struct, not polymorphic; it carries one member per backend (lua_State, wasm::Instance, js::Instance).
- Dispatch on manifest `runtime` happens in plugin_call.cpp; load wiring in plugin_load.cpp.
- Each language lives in its own namespace (plugin::lua / plugin::js / plugin::wasm), not a class.

## Industry patterns considered
1. Escape hatch + narrow C ABI (Redis modules, Envoy/wasm, Neovim) — most common; sandbox is the language's own.
2. Stable Wasm ABI + runtime per language in plugin (proxy-wasm, Extism, Spin) — N languages, one backend; needs per-language tooling.
3. Polymorphic backend interface in host (GObject libpeas, Unreal IModuleInterface) — the interface question.
4. Out-of-process JSON-RPC/gRPC (LSP, VS Code extension host, Terraform providers) — crash isolation, heavier plumbing.

## Decision (lazy)
Do NOT introduce `PluginBackend` now. Adding a backend today touches an explicit short list: `Runtime`, `plugin_load.cpp`, `plugin_call.cpp`, `plugin_internal.hpp` (+ the backend namespace). With 3 backends and a stable JSON ABI, a vtable interface adds boilerplate without making wiring easier; tracking feature parity is better read from plugin_internal.hpp + the backend namespaces.

## Revisit when
- A 4th runtime is planned (e.g. Rhai, a real out-of-process host), or
- A backend needs per-instance state whose lifecycle diverges enough that the struct's shared `mu`/`health` no longer fits, or
- Out-of-process isolation is required — then implement `PluginBackend` and keep in-process backends behind it.

## Enrichment note
Enrichment plugins use the same `Runtime`/dispatch; they are tagged `enrichment` capability, never enter the scraper registry, and share host tools (fs/http/storage/crypto).
