---
type: constraint
title: Lua host tools require the clang toolchain
description: "Build with clang only: GCC renders every same-signature lambda to one type name and sol2 3.5.0 keys functor metatables on that name (upstream #1743), so a GCC build dispatches wrong destructors at lua_close and segfaults on plugin teardown / /plugins/reload."
tags: [lua, sol2, clang, gcc, toolchain, segfault]
generated: { by: agent/mcp, at: "2026-11-16T10:19:55Z" }
status: stable
---

## Rule

`src/plugin/lua/lua_runtime.cpp` must be compiled with clang (`xmake.lua:5 set_toolchains("clang")`).
Do not switch the project toolchain to gcc without first reworking `install_host_tools` so that all
lambdas sharing a signature have an identical, trivially-destructible layout.

## Why

sol2 3.5.0 builds its per-callable metatable key from `__PRETTY_FUNCTION__`. GCC prints the same text
for every lambda of a given signature (`main()::<lambda(int)>`); clang prints the source location
(`(lambda at file:line:col)`). Identical keys make `user_alloc_destroy<T>` run the wrong destructor
when a state is closed — a wild read for a functor holding a by-value `glz::generic`/`std::map`.

## Symptom

Daemon SIGSEGV on `POST /plugins/reload` or on any `lua_close`; under ASan, an abort inside
`luaC_freeallobjects` -> `~functor_function` -> `~glz::generic`.

## Scope

Lua runtime only. WASM (wasm3) and JS (quickjs-ng) runtimes register host tools through their own
bindings and are not affected by sol2's naming.

Full analysis: `findings/sol2-lua-functor-teardown-corruption`.
