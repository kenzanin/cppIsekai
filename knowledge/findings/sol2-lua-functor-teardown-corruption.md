---
type: Finding
title: sol2 3.5.0 functor teardown corruption is GCC-only (clang builds are safe)
description: "Resolved: sol2 3.5.0 keys functor metatables on __PRETTY_FUNCTION__, which GCC renders identically for every same-signature lambda (clang includes source location) -> wrong destructor at lua_close. cppIsekai builds with clang, so no collision; verified green teardown + reload. Found and fixed a real dangling [&cfg]/[&params] capture in install_host_tools."
tags: [sol2, lua, gc, segfault, cppIsekai, upstream, asan, clang, gcc, toolchain]
generated: { by: agent/mcp, at: "2026-11-16T10:19:55Z" }
status: stable
---

## Status: resolved (toolchain-specific)

The crash is a **GCC-only** problem. cppIsekai builds with clang (`xmake.lua:5 set_toolchains("clang")`),
and under clang the collision does not exist. The daemon SIGSEGV and the ASan aborts were all produced
from a GCC-toolchain build.

## Root cause (confirmed)

sol2 3.5.0 keys its functor metatables on a **type name derived from `__PRETTY_FUNCTION__`**. GCC renders
every same-signature lambda to the *same* name; clang includes the source location:

```
g++      pretty A : const char* pf() [with T = main()::<lambda(int)>]
g++      pretty B : const char* pf() [with T = main()::<lambda(int)>]     <-- identical
clang++  pretty A : const char *pf() [T = (lambda at lambda_key.cpp:10:14)]
clang++  pretty B : const char *pf() [T = (lambda at lambda_key.cpp:11:14)]  <-- distinct
```

Identical keys mean two lambdas with the same signature but different captures share one metatable
entry, so `user_alloc_destroy<T>` at `lua_close` runs the destructor for the wrong T (upstream sol2
#1743). In this codebase that pair is `config.get` (captured a `glz::generic`) and `plugin.param`
(captured a `std::map`) — same signature `(const std::string&, sol::this_state) -> sol::object`,
completely different layout, hence the wild read inside `~glz::generic`.

## Evidence after the switch to clang

- `tests/test_plugin` 24/24 cases, 170 assertions, exit 0, no teardown abort.
- Daemon (`build/linux/x86_64/release/cppIsekai`, clang 23.1.5): `POST /plugins/reload` 8x over two
  boots, always HTTP 200, process alive, library still 36 titles.
- `coredumpctl list` has no cppIsekai entries (only unrelated `mold` SIGBUS).

## Real bug found while reproducing: dangling config/params captures

`install_host_tools` used to capture `[&cfg]` / `[&params]`. Those references bind to `rt` — a
`Runtime` local to `scan_dir` — and `plugin_load.cpp` moves only `info/lua/wasm/js` into the shared
`Runtime`, so `rt.cfg` dies when `scan_dir` returns. Every later `config.get` / `plugin.param` call
read freed memory (mangadex reads its OAuth credentials this way at load).

Fixed by capturing both **by value** (a copy per plugin load, which is what makes the value outlive
`rt`). Note the tension with workaround (1) below: by-value `glz::generic` is exactly the payload that
crashes under GCC — that is another reason the clang-only rule matters.

## What was ruled out (do not re-tread)

1. System sol2 vs xrepo GitHub tarball: byte-identical crashes at the same frame.
2. `LUA_GCCOLLECT` x2 before `lua_close`: no change.
3. Host/Store declaration order in test_htmltext / test_plugin: no change.
4. `glz::generic` copy ctor: 20000-iteration stress under ASan passes.
5. Lua version: not implicated — the corruption is in sol2's dispatch layer, not Lua GC.
6. valgrind: no capture (signal/timing perturbation, not evidence of safety).
7. Our RAII captures' lifetime: the innermost frame is sol2's metatable-driven `__gc` dispatch.

## If a GCC build is ever needed

The ranked options stand: (1) make every lambda of a shared signature capture the same trivial layout
(`shared_ptr` only, e.g. one `shared_ptr<HostCtx>`), (2) carry the upstream #1743 fix, or (3) collapse
the host tools onto a single `lua_CFunction` trampoline. None of these are needed under clang, which is
why they are not implemented — see `constraints/lua-host-tools-require-clang`.

## Notes for the team

- A GCC build of a Lua-plugin daemon is unsafe, not merely slow: `/plugins/reload` and any state
  teardown can segfault. The `#error`-free guard is the comment block at the top of
  `install_host_tools` (`src/plugin/lua/lua_runtime.cpp`).
- `constraints/sol2-lua-version` still governs why Lua is pinned to 5.4.8.

# Related Concepts
- [Lua host tools require the clang toolchain](../constraints/lua-host-tools-require-clang.md): The finding's resolution is this constraint: the corruption only exists under GCC, and the project toolchain is clang.
