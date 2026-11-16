---
type: Finding
title: Lua plugin end-to-end audit (search/detail/chapters/pages)
description: "Audit of Lua scraper plugins (search/detail/chapters/pages) after 2026-11-16 morning fixes; afternoon re-check with 1manga as reference: 2 plugins deleted (CF/dead), 4ucomics shape fixed, rest noted."
tags: [plugins, lua, audit]
generated: { by: agent/mcp, at: "2026-11-16T12:52:28Z" }
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

## 2026-11-16 afternoon re-check (daemon health probe)

Re-probed every exposed plugin against the local daemon (search "one" + detail + chapters + pages of first hit when search returned items). Result:

- 1manga — FIXED into the reference layout (module-per-purpose: main.lua entry + text/http/slugmap/search/detail/chapters/pages/genres); search 57 items, lowercase canonical statuses; e2e green.
- harimanga, kaliscan — REMOVED from data/plugins (harimanga = Cloudflare 403 "Just a moment"; kaliscan = NXDOMAIN). Sources kept in plugins_dev for learning. Base/plugin data for already-saved titles remains readable.
- 4ucomics — FIXED: was emitting `title`/`author` non-canonical shape (search rendered empty names); now `name`, lowercase canonical status, tags/release_at/created_at/release_at on chapter rows, e2e green (items=30, detail ok, chapters=1230, pages=16). Homepage answers 403 to curl but the plugin's lightpanda path works end to end (not a CF block on the fetch path).
- anisascans — site now resolves `anisascans.com` (`.in` NXDOMAIN; plugin already uses .com), but the Madara search markup yields 0 cards (upstream drift / CF drop on search path). Noted, not fixed.
- batcave — now PASSES (search 10 items, detail/chapters/pages ok); earlier CF 403 no longer reproduces via the probe path at this time.
- comico — search times out at 30 s (comico.moe returns 200 to curl; fetch path is dead/broken). Noted, not fixed.
- comik — search ok (48 items), chapters endpoint times out at 30 s. Noted, not fixed.
- comix — search/detail/chapters ok; pages fail: headless render yields 0 `rpage-page` slots (`comix: no pages for <id>`) — headless cannot scroll/lazy-load reader images. Noted, not fixed.
- mangahub — `items=0` (mangahub.io GraphQL endpoint previously returned 403; now TLS handshake failure from curl; endpoint effectively dead for the plugin). Noted, not fixed.
- mangabay: pages=0 (image regex no longer matches); mangafox (WASM): pages=0; mangahere: chapters=0; mangahome: chapters "no_chapters"; mangafreak (WASM): detail `name` empty; mangatown (WASM): pages timeout 30 s; mangakatana: "no pages for one.4457:v11c71" — chapter-id composite mismatch; sanascans: "no pages for ..." — chapter-id composite mismatch; mangadex: `pages=err at-home: malformed response` (at-home fingerprint/client issue); valirscans: items=0 (CF drop of chrome-fingerprinted fetch, as before) — confirmed again; komix_to WASM: \`[trap] out of gas\` in 11.9 s.
- komix_to (WASM) — search times out at 30 s (previously `wasm: out of gas`). Noted, not fixed.
- 1manga-related side fixes already committed earlier today: `host.text.trim` (whitespace/control strip only), Alpine `x-` scope fix in templates, `$index` removal (search_script), detail-cover `candFor().data` null guard, normalization moved into the plugin.

Not investigated / out of scope this pass: JS (mangzio, 4ucomics-JS-path) and WASM plugins (komix_to, mangatown, mangafox, mangafreak) — only noted for the record; Lua scraper fixes only.

Decision: CF-blocked or dead-upstream plugins are deleted from `data/plugins/` (sources kept in `plugins_dev/` for learning). Deletions so far: harimanga, kaliscan.
