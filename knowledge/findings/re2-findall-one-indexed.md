---
type: Fact
title: "host.re2.find_all is 1-indexed in Lua: m[1] = full match, groups start at m[2]"
description: "sol2 marshals the C++ vector<vector<string>> [full, g1, ...] to a 1-based Lua table, so plugins written with Go-style indexing are off by one."
tags: [plugins, lua, re2, convention]
generated: { by: agent/mcp, at: "2026-11-16T06:49:34Z" }
status: stable
---

## Convention

`host.re2.find_all(pattern, body)` returns `{ { full, g1, g2, ... }, ... }` marshalled by sol2 as a 1-based Lua table: `m[1]` = whole match, `m[2]` = capture group 1, `m[3]` = group 2.

Correct uses:
- `local card = m[1]` then parse `card` with `find1`/`string.match` (full-block intent).
- Groups accessed as `m[2]..m[n+1]`.

Wrong (Go-style): using `m[1]` as group 1. Fixed 2026-11-16 in mangaread, mangasushi, topmanhua, anisascans, sanascans (search + chapter list), valirscans json-ld, en-thunderscan card block.

## Audit pattern

Grep `m[1]` / `g[1]` / `row[1]` next to find_all loops; each is a bug unless the value is used as the raw match block. `host.re2.find` follows the same shape (`m[1]` = full match, `m[2]` = group 1).
