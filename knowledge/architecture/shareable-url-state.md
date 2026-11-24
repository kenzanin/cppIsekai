---
type: decision
title: "Every page mirrors its view state into the URL (reader parity, replaceState only)"
description: "Library grid, search, detail chapter list and plugins filter all read their state from the query string at boot and rewrite it with history.replaceState on every change (same rule as the reader): URL is always shareable/refreshable, no history entries are added, and no GET mutation route exists. Grid params: q, plugin, sort, dir, dup=1, f=chip:show|hide comma-list, page (>1 only); URL beats saved grid_prefs. Detail: chpage (1-based, clamped after chapters load), hidden=1. Search: plugin, q, tags, page (pushState -> replaceState). Plugins: q. Tests: tests/test_gridjs.cpp has URL shims + a round-trip subcase; goldens re-locked."
tags: [web, url-state, shareable, reader-parity, alpine, grid]
generated: { by: agent/mcp, at: "2026-11-24T13:12:15Z" }
status: stable
---


