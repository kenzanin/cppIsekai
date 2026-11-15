---
type: constraint
title: sol2 3.5.0 rejects Lua 5.5 from Arch pacman
description: "sol2 3.5.0 compat header rejects Lua 5.5 (LUA_ERRGCMM undeclared). Since 2026-11-15 both sol2 and lua come from xrepo: sol2 3.5.0 with configs={includes_lua=false}, lua pinned 5.4.8 because bare xrepo 'lua' resolves to 5.5"
tags: [sol2, lua, constraint, xrepo]
generated: { by: agent/mcp, at: "2026-11-15T15:47:10Z" }
status: stable
---

Updated 2026-11-15: system lua (pkgconfig::lua54) is no longer used. xrepo lua is pinned to 5.4.8 in xmake.lua because the xrepo default resolves to 5.5.x, which sol2 3.5.0's compat header rejects (LUA_ERRGCMM undeclared, 5.1-5.4 only). sol2 is pulled from xrepo with configs={includes_lua=false} so sol2's recipe does not pull its own lua dependency. Do NOT add_requires bare 'lua' without the version pin or it resolves to 5.5.
