---
type: constraint
title: sol2 3.5.0 rejects Lua 5.5 from Arch pacman
description: "Arch system lua is 5.5.1; sol2 3.5.0 compat header rejects Lua versions other than 5.1-5.4 (LUA_ERRGCMM undeclared). Fix: keep system lua54 5.4.9 via pkgconfig::lua54, and pull sol2 from xrepo with configs={includes_lua=false} so sol2's recipe does not pull xrepo 'lua' (which resolves to pacman lua 5.5). Do NOT add_requires bare 'lua' from xrepo without system=false or it resolves to pacman 5.5."
tags: [sol2, lua, constraint, xrepo]
generated: { by: agent/mcp, at: "2026-11-13T00:44:17Z" }
status: stable
---


