---
type: constraint
title: "Chapter state keys: one canonical (decoded) key space; rollup owns total_ch"
description: "Host state routes percent-decode path ids before keying plugin_state, and apply_read_rollup refreshes total_ch on every path - row-served opens never sync, so totals live or die by the rollup."
tags: [reader, state, encoding, regression, host]
generated: { by: agent/mcp, at: "2026-11-15T17:11:03Z" }
status: stable
---

## Rule 1 - one key space
Chapter state lives under `host/{read,skip,total}/<id>`. Ids reach the host in TWO shapes: path params stay percent-encoded (crow does not decode `<string>`, `Kcm9%3ASuDXw`), body/query ids arrive raw. Every state writer/read route MUST `plugin::url_decode` the path id first (src/http/http_api_state.cpp read+skip) so reader positions, read-toggles, /chapter/state and the rollup all land on the same key. Symptom when violated: read toggle "disappears" on refresh (atsumaru/mangafreak style ids with ':').

## Rule 2 - rollup owns total_ch
`apply_read_rollup` (src/http/api_ctx.hpp) runs on EVERY detail path incl. row-served, which never runs `sync_saved_stats`; it therefore refreshes `total_ch` from the chapters envelope it already holds (via `saved_stats_update(plugin, slug, "", n)`, empty status keeps category bits). Done = read_ch >= total_ch composes after.

## Rule 3 - absent envelope keeps stored stats
`saved_stats_update` treats negative total_ch as "unknown, keep stored" - a sync with a missing chapters cache must not zero the count (regression: mangadex row stuck total 0, done unreachable).

## Housekeeping
Legacy encoded keys are merged max-wins into raw form; no `%3A` state keys may be created going forward. Tests: test_http_enrich "apply_read_rollup heals total_ch", test_store negative-total keep. Landed 349ab28.
