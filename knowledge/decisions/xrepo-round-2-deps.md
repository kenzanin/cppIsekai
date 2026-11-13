---
type: decision
title: "Second xrepo round: pkgconfig deps + utf8proc NFKD tokenizer"
description: "Migrated lua (pkgconfig::lua54 system), re2, spdlog, zlib, sqlite3 to xrepo (re2 pinned 2025.11.05, lua forced system because Arch pacman lua is 5.5 incompatible with sol2 3.5.0). Added utf8proc 2.11.3 for Unicode NFKD tokenisation replacing hand-rolled Latin-only fold. All pins locked in xmake-requires.lock. 17/17 tests green."
tags: [xrepo, dependencies, utf8proc, re2, tokenizer]
generated: { by: agent/mcp, at: "2026-11-13T00:40:08Z" }
status: stable
---


