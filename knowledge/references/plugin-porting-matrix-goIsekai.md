---
type: Reference
title: "goIsekai -> cppIsekai plugin porting matrix"
description: "Matrix mapping goIsekai example plugins to cppIsekai plugin implementations, ABI differences, and what remains to port."
tags: [plugin, goisekai, porting, matrix, abi]
generated: { by: agent/mcp, at: "2026-11-13T01:27:15Z" }
status: stable
---

# Plugin porting matrix: goIsekai -> cppIsekai

## Source corpus (read-only reference)

- Repo: ~/tmp/goIsekai — user's first golang experiment, test harness + stack trial.
- cppIsekai is the cleaner successor; goIsekai is NOT a dependency, reference only.
- Example plugins: ~/tmp/goIsekai/examples/plugins/<runtime>/<site>/main.lua
  - runtimes: lua/ (~40 sites), js/ (~40 sites), wasm/, native/, yaegi/
- HAR captures: ~/tmp/goIsekai/tmp/*.har + ~/tmp/goIsekai/tmp/har/ (real traffic for verification)

## goIsekai plugin ABI (Lua)

| Export | Args | Returns |
|--------|------|---------|
| PLUGIN table | — | {name, site_url, logo, thumb_ratio, needs_human_verify, ...} |
| search_manga(arg) | {"query","page","genres"} | JSON array of manga |
| get_genres() | — | JSON array of {name, slug} |
| get_manga_detail(arg) | "manga-id" | detail object |
| get_chapter_list(arg) | "manga-id" | chapter array (newest-first) |
| get_page_list(arg) | "chapter-id" | page array {index, url, headers} |

Host tools: host.json.*, host.text.*, log.*, host.crypto.vrf_sign(...), util.http_get() (plugin-side wrapper).

## cppIsekai plugin ABI (current)

JSON-text-in/out, single host wrap point call_cached in src/http/http.cpp.

Host tools: http.fetch(url, opts), storage.get/set(key), log.*, crypto.*.

## Ported plugins (cppIsekai plugins_dev/lua/)

| Plugin | Source | Status | Notes |
|--------|--------|--------|-------|
| mangafire | goIsekai examples/plugins/lua/mangafire | ported | VRF-signed JSON API; VRF tables plugin-side |
| atsumaru | goIsekai examples/plugins/lua/atsumaru | ported | genres.lua, main.json, logo.png, main.lua |
| mangadex | goIsekai examples/plugins/lua/mangadex | ported | public JSON API at api.mangadex.org |
| enrichment-mangadex | — | ported | enrichment provider |
| enrichment-mangaupdates | — | ported | enrichment provider |

## Host-tool gaps (block direct port)

| Gap | Source | cppIsekai equivalent | Blocked? |
|-----|--------|----------------------|----------|
| host.crypto.vrf_sign(path, sign, stages) | goIsekai | host.crypto.vrf_sign | partial — see host-crypto-vrf-sign |
| util.http_get() wrapper | plugin-side | http.fetch host tool | handled in plugin |
| host.text.date_to_iso() | goIsekai | plugin-side date handling | plugin-side |
| OAuth2 token persistence | goIsekai | storage.get/set | plugin_state table |

## What remains to port

- ~35 more sites from goIsekai lua/ runtime (mangapill, 1manga, anisascans, batcave, comico, comik, comix, comizy, dynastyscan, fenyk, kaliscan, lhtranslation, madaradex, mangabay, mangabuddy, mangade, mangatown, sanascans, weebcentral, zeroscann, …)
- JS (quickjs) runtime ports per decisions/plugin-port-js-wasm
- WASM (wasm3) runtime ports

## Verification approach

- Mine selectors/endpoints from goIsekai HAR captures
- Verify ported plugin behavior against real traffic via HAR replay
- Compare runtime approaches (Lua sol2 vs quickjs-ng vs wasm3)

## Do not merge

- goIsekai has its own knowledge/ OKF bundle and openspec/ — read-only reference, do not merge bundles.
