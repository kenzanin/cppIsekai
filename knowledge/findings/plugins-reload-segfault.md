---
type: Finding
title: /plugins/reload segfaults the cppIsekai daemon
description: "Historical: /plugins/reload used to SIGSEGV because the daemon was built with GCC (sol2 lambda-name collision). Resolved by the clang toolchain — reload now returns 200 over repeated calls; do not follow the old 'never call reload' rule."
tags: [plugins-reload, segfault, sol2, lua, asan]
generated: { by: agent/mcp, at: "2026-11-16T10:20:13Z" }
status: deprecated
---

## Resolved — do not act on the old rule

`POST /plugins/reload` is safe on a clang build (the project default, `xmake.lua:5`). The 2026-11-16
crash came from a **GCC-toolchain** binary: GCC gives every same-signature lambda one type name, sol2
3.5.0 keys functor metatables on that name, and `lua_close` then ran a wrong destructor. Root cause and
the clang clang-vs-gcc evidence: `findings/sol2-lua-functor-teardown-corruption`; the standing rule:
`constraints/lua-host-tools-require-clang`.

## Verified after the toolchain was settled

- `POST /plugins/reload` -> 200 with `{"count":30}`, repeated 8x across two boots, daemon alive each time,
  library still listing 36 titles.
- `tests/test_plugin`: 24/24 cases, no teardown abort.
- `coredumpctl list` shows no cppIsekai entries.

## Original evidence (GCC build, kept for the record)

- 2026-11-16 ~13:42 WIB pid 908300 (SIGILL), pid 989001 (SIGSEGV) on plugins/reload.
- ASan `releasedbg`: test_htmltext & test_plugin aborted at Host teardown —
  `luaC_freeallobjects -> user_alloc_destroy<functor_function<...config-get-lambda>>` (stack_core.hpp:460)
  `-> ~glz::generic_json -> _M_reset -> __do_visit` -> wild READ.
- Same signature on system sol2 and the xrepo tarball, so the corruption was in sol2's dispatch, not the
  package source, flags, Lua version, or our captures.
- Rule that applied at the time: do not call POST /plugins/reload, restart the daemon instead.

## Surviving operational note

Daemon ignores SIGTERM/SIGINT; SIGKILL required (SQLite WAL survives). Daemon log is tail-only.

# Related Concepts
- [sol2 3.5.0 functor teardown corruption is GCC-only (clang builds are safe)](sol2-lua-functor-teardown-corruption.md): This was the surface symptom; the toolchain-rooted analysis lives there.
