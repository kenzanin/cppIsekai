---
type: decision
title: "Second xrepo round: pkgconfig deps + utf8proc NFKD tokenizer"
description: "Migrated lua (pkgconfig::lua54 system), re2, spdlog, zlib, sqlite3 to xrepo (re2 pinned 2025.11.05, lua forced system because Arch pacman lua is 5.5 incompatible with sol2 3.5.0). Added utf8proc 2.11.3 for Unicode NFKD tokenisation replacing hand-rolled Latin-only fold. All pins locked in xmake-requires.lock. 17/17 tests green."
tags: [xrepo, dependencies, utf8proc, re2, tokenizer]
generated: { by: agent/mcp, at: "2026-11-14T10:31:34Z" }
status: stable
---

# Related Concepts
- [re2 xrepo version pin](../constraints/xmake-re2-pin.md): Constraint record for the re2 pin.
- [sol2 3.5.0 rejects Lua 5.5 from Arch pacman](../constraints/sol2-lua-version.md): Lua version constraint behind the system lua pin.
- [utf8proc 2.11.3 C API surface](../constraints/utf8proc-2.11-3-api.md): utf8proc 2.11.3 API constraint.
- [dummy seed plugins removed](dummy-seed-removal.md): Removal of dummy seed data.
