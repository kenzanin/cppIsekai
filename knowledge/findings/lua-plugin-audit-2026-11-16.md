---
type: Finding
title: Lua plugin end-to-end audit 2026-11-16 (search/detail/chapters/save)
description: "Audit of all 29 Lua scraper plugins through the daemon routes after fixes: 21 pass fully, 8 fail now (all upstream blocks, no code bug in 7 of them)."
tags: [plugins, lua, audit]
generated: { by: agent/mcp, at: "2026-11-16T06:49:50Z" }
status: stable
---

## Scope

`bash-audit`TSV run 2026-11-16 (~13:28-13:55 WIB). Exclusions: `enginetest` (probe harness), `enrichment-mangadex` / `enrichment-mangaupdates` (metadata enrichment, not searchable). Probe: search `isekai` then `one piece`, first result -> POST /api/refresh -> /api/detail + /api/chapters, save round-trip with a synthetic `__audit<ts>__` slug.

## Fixed and re-verified green (search -> refresh -> detail -> chapters -> save round-trip)

4ucomics, atsumaru, comico (362 items, chapters=0 for the sampled title), comik (444 ch), comix, dynastyscan, fanfox, madaradex, mangabay, mangabuddy, mangadex, mangafire, mangahere, mangahome, mangakatana (was: detail 404; now 78 ch), mangaread (regex bug), mangasushi (regex bug), sanascans (group shift; 58 items), topmanhua, violetmanga, en-thunderscan (was: silently empty search; 10 items), comizy (was: 404; separator `|`->`.`).

## Still failing (all upstream, not fixable in code)

- 1manga — GraphQL `403` on search ("refreshing key" path), 0 items.
- mangahub — GraphQL `403`, 0 items.
- anisascans — `anisascans.in` DNS does not resolve (search fails); known upstream block.
- kaliscan — `kaliscan.me` DNS does not resolve (search fails); manifest notes flag the domain as dead, kept for parity.
- harimanga — `www.harimanga.cc` answers `403` to the plugin's fetch path.
- batcave — Cloudflare "Just a moment" JS challenge (403 page) on `/search/isekai`; the DLE PoW guard (`dle.lua`) never sees a token because the challenge page itself blocks the impersonate-403; 0 cards parsed from the challenge page, ok:true total:0.
- valirscans — upstream works from plain curl (200 JSON) but CF drops the chrome116-fingerprinted fetch with `307 -> /`; plugin then decodes nothing, ok:true total:0. `.org` move already reflected in BASE.
- valirscans json-ld Book parse also fixed (m[1] -> m[2]), still blocked by CF at fetch level.

## Notes

- save round-trip OK on every plugin (route is plugin-agnostic; `__audit*__` rows verified added then removed).
- /api/detail for unsaved slugs is call_cached-served; refresh failures leave cache untouched by design.
- comico chapters=0 on sampled novel — possibly legitimate; not investigated.
- valid_title_pair blocks real slugs — see findings/valid-title-pair-charset (open decision).
- /plugins/reload crashes the daemon — see findings/plugins-reload-segfault (restart instead).
