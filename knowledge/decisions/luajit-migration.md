---
type: Decision
title: "LuaJIT migration: skipped (not faster, higher risk)"
description: "LuaJIT migration considered and explicitly skipped: speed barely improves, cost is build-wiring churn; host crypto via openspec chosen instead."
tags: [lua, luajit, deps, plugin, sol2]
generated: { by: agent/mcp, at: "2026-11-12T05:25:08Z" }
status: stable
---

User-approved 2026-11-12: do NOT migrate the plugin runtime from Lua 5.4 to LuaJIT.

## What was checked
- sol2 vendored = 3.5.0 (third_party/sol2), has lua_version.hpp with SOL_USING_CXX_LUAJIT support
- System LuaJIT 2.1.1794214635 present (pkg-config luajit, headers /usr/include/luajit-2.1)
- src uses only stable C API: luaL_requiref(LUA_STRLIBNAME/TABLIBNAME/MATHLIBNAME), lua_sethook(LUA_MASKCOUNT), sol::protected_function, safe_script_file - all LuaJIT-compatible
- No 5.4-only syntax observed in plugins_dev/lua (no <const>/<close>, no integer division, no native bitwise/utf8 use); plugins use Lua patterns / host.re2 (RE2), not Lua 5.4 features

## Why skipped
- LuaJIT is faster on interpreter/JIT-heavy pure-Lua workloads; plugin call time is dominated by host.http.fetch I/O, so the speedup is marginal
- Migration cost: CMakeLists pkg_check_modules(lua5.4), xmake pkgconfig::lua5.4 (all targets incl. tests), sol2 SOL_USING_CXX_LUAJIT + include path wiring, plus runtime parity risk across lua/js/wasm
- VRF crypto work was prioritized instead (see references/goisekai-abi-mapping); mixing both changes in one step would blur attribution

## If revisited later
Treat as its own openspec change: switch lua package in CMakeLists + xmake to luajit, compile sol2 against SOL_USING_CXX_LUAJIT, run full doctest suite + plugin smoke (mangadex/atsumaru), verify ABI (handle()/envelope) is unchanged.
