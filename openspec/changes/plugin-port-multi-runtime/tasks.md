# Tasks: plugin-port-multi-runtime

## Phase 1 — Lua (sol2): remaining goIsekai sites

### Findings (task 1.1)
- goIsekai `lua/`: 38 sites total. cppIsekai already ports: atsumaru, mangadex, mangafire (+ enrichment-mangadex, enrichment-mangaupdates not in goIsekai list).
- Remaining to port: 35 sites.
- Cloudflare-affected (skip): onisaga, weebcentral, fenyk, mangade, zeroscann, mangataro, lhtranslation (per goIsekai knowledge log: CF challenge 403, empty-search, mid-request failures).
- Non-CF, ready to port: 1manga, anisascans, batcave, comico, comik, comix, comizy, dynastyscan, en-thunderscan, fanfox, harimanga, htmlscrape, kaliscan, madaradex, mangabay, mangabuddy, mangafreak, mangahere, mangahome, mangahub, mangakatana, mangaread, mangasushi, mangataro(?), mangatown, sanascans, topmanhua, valirscans, violetmanga, zeroscann(?).
- dummy, htmlscrape are test/scaffold sites.
- **Host-tool parity gap (per D1a): CLOSED by host-tool-gap change** — P1–P7 landed (`host.plugin.param`, `host.http.fetch_body/get_json/post_json`, `host.text.*`, `host.re2.find_all/gmatch/replace/quote/find_index`, `host.codecs.*`, `host.crypto.sha256/md5/hmac/aes_gcm`; P9 `host.html.xpath` in progress). Porting now reduces to copy-paste + prefix substitution (see `references/host-tool-gap-analysis`).

- [x] 1.1 List goIsekai `lua/` sites; cross-reference cppIsekai `plugins_dev/lua/`; mark Cloudflare-blocked
- [ ] 1.2 Port each remaining Lua site to `plugins_dev/lua/<site>/` (main.json + main.lua + genres.lua + logo.png)

  **Pilot landed:** anisascans ported (manifest+logo already present). Lua loads clean through the host loader (genres round-trips `{ok,data}` ✓, host.re2/http.fetch/url_encode all exercised). Live search currently blocked upstream (site served a JS-challenge/redirect page for this IP → effectively CF-protected, already excluded from porting scope). Remaining sites = same port shape, minus anisascans-style regex paths.
- [ ] 1.3 HAR-verify each Lua ported plugin against `~/tmp/goIsekai/tmp/`
- [ ] 1.4 Full test suite green; daemon restart; Lua plugin catalog loads

## Phase 2 — JS (quickjs-ng): port goIsekai JS sites

- [x] 2.1 JS loader host-tool bindings confirmed present (`src/plugin/js/js_bindings.cpp`: http.fetch/fetch_body/get_json/post_json, re2.*, html.*, text.*, codecs.*, crypto.* (vrf+sha256/md5/hmac/aes_gcm), config.get, storage, log, require, url_*). NOT re-wire. Smoke-test one JS site.
- [x] 2.2 Smoke-test one JS site end-to-end
  pilot: mangatown
- [ ] 2.3 Port remaining goIsekai JS sites to `plugins_dev/js/<site>/` (main.json + main.js)
- [ ] 2.4 HAR-verify each JS ported plugin
- [ ] 2.5 Full test suite green; JS runtime loads plugins

## Phase 3 — WASM (wasm3): port goIsekai WASM sites

- [x] 3.1 Choose toolchain: wasi-sdk vs odin (record decision) — **DECIDED: wasi-sdk** (design D7; host pacman `wasi-sdk-git`, clang → wasm32-wasi, C ABI maps 1:1 to wasm3 `env.*` imports; odin dev-channel risk) — wasm3 loader + imports present (`src/plugin/wasm/wasm_imports.cpp`); site plugins mostly Go/odin compiled → toolchain decision matters
- [ ] 3.2 wasm3 loader host-tool bindings confirmed present (http.fetch/fetch_body/get_json/post_json, re2.*, html.*, text.*, codecs.*, crypto.* (vrf+sha256/md5/hmac/aes_gcm), url_*)
- [ ] 3.3 Smoke-test one WASM site end-to-end
- [ ] 3.4 Port goIsekai WASM sites to `plugins_dev/wasm/<site>/` (main.json + main.wasm)
- [ ] 3.5 HAR-verify each WASM ported plugin
- [ ] 3.6 Full test suite green; WASM runtime loads plugins

## Phase 4 — Verification + gaps
- [ ] 4.1 Document host-tool gaps per runtime (Lua/JS/WASM)
- [ ] 4.2 List Cloudflare-blocked sites skipped (not ported)
- [ ] 4.3 Full suite green; daemon restarted; all three runtimes load plugins
