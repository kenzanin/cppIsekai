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
- [x] 1.4 Full test suite green; daemon restart; Lua plugin catalog loads

## Phase 2 — JS (quickjs-ng): port goIsekai JS sites

- [x] 2.1 JS loader host-tool bindings confirmed present (`src/plugin/js/js_bindings.cpp`: http.fetch/fetch_body/get_json/post_json, re2.*, html.*, text.*, codecs.*, crypto.* (vrf+sha256/md5/hmac/aes_gcm), config.get, storage, log, require, url_*). NOT re-wire. Smoke-test one JS site.
- [x] 2.2 Smoke-test one JS site end-to-end
  pilot: mangatown
- [x] 2.3 Port remaining goIsekai JS sites to `plugins_dev/js/<site>/` (main.json + main.js) -- goIsekai js = dummy (scaffold) + mangzio only; mangzio live-verified
- [x] 2.4 HAR-verify each JS ported plugin -- mangzio: live-daemon verified search/detail/chapters/pages/genres (no mangzio.har exists in goIsekai/tmp); mangatown: 2.2 smoke
- [x] 2.5 Full test suite green; JS runtime loads plugins

## Phase 3 — WASM (wasm3): port goIsekai WASM sites

### Findings (task 3.7) — mangafox reader dead end to end
Found during the 6-plugin chapter-nav sweep (2026-11-15), not a regression in `/api/pages`:
- `plugins_dev/wasm/mangafox/main.cpp:307` builds `.../manga/<slug>/<ch>/1.html`; fanfox needs
  `.../manga/<slug>/c<ch>/1.html` (probe: `c001` → 200, `001` → 404).
- Even with the URL fixed, the mobile chapter page is today a ~10 KB JS-rendered shell with no
  `chapterid` / `imagecount` markers, so the regexes at lines 309-310 match nothing — the guest needs a
  different source for the page list (the old `chapterfun.ashx` endpoint or the desktop reader page).
- Symptom: `/plugin/mangafox/pages` → `{"ok":false,"error":"fetch failed"}` and
  `/api/pages/mangafox/...` 404/500 for every chapter, while search/detail/chapters still work from cache.
- A sibling numbering bug in the same guest was fixed separately (chapter 24b → 24 in c8157d8).

- [x] 3.1 Choose toolchain: wasi-sdk vs odin (record decision) — **DECIDED: wasi-sdk** (design D7; host pacman `wasi-sdk-git`, clang → wasm32-wasi, C ABI maps 1:1 to wasm3 `env.*` imports; odin dev-channel risk) — wasm3 loader + imports present (`src/plugin/wasm/wasm_imports.cpp`); site plugins mostly Go/odin compiled → toolchain decision matters
- [x] 3.2 wasm3 loader host-tool bindings confirmed present (http.fetch/fetch_body/get_json/post_json, re2.*, html.*, text.*, codecs.*, crypto.* (vrf+sha256/md5/hmac/aes_gcm), url_*)
- [x] 3.3 Smoke-test one WASM site end-to-end — **mangafreak** (search 7 / detail / chapters 1204 / pages 13, all ok:true live)
- [ ] 3.4 Port goIsekai WASM sites to `plugins_dev/wasm/<site>/` (main.json + main.wasm)
- [ ] 3.5 HAR-verify each WASM ported plugin
- [x] 3.6 Full test suite green; WASM runtime loads plugins
- [ ] 3.7 Repair the mangafox WASM guest's reader path (see Findings above): switch the chapter URL to
      `c<ch>/1.html` and replace the `chapterid`/`imagecount` extraction with a source that still exists
      (desktop reader page or the chapterfun endpoint); rebuild the `.wasm`, then verify live that
      `/api/pages/mangafox/<slug>/<ch>` returns pages and the first image loads through `/cache/img`

## Phase 4 — Verification + gaps
- [x] 4.1 Document host-tool gaps per runtime (Lua/JS/WASM) -- parity closed by host-tool-gap (P1-P7). Observed while porting: JS guest loop budget kOpsPerCall=200k -> bulk array/string extraction must run host-side re2 (mangzio chapters 690KB); host.text.trim casefolds (genre-label casing); Lua/WASM: no gaps hit
- [x] 4.2 List Cloudflare-blocked sites skipped (not ported) -- per goIsekai knowledge log CF-challenge failures: weebcentral, fenyk, mangade, zeroscans, mangataro, lhtranslation (onisaga not present in goIsekai tree)
- [x] 4.3 Full suite green; daemon restarted; all three runtimes load plugins
- [ ] 4.4 Reader-path verification for every ported plugin, not just search/detail: `/api/chapters` returns
      a usable number sequence (no placeholder 0, no bogus ordering) and `/api/pages` returns pages with a
      working image URL for at least one chapter — a green build and a working detail page have repeatedly
      masked dead readers (mangafox today, see 3.7; mangafire phantom chapter 0, fixed 1e24d06)
