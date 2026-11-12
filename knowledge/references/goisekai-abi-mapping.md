---
type: Reference
title: goIsekai to cppIsekai plugin ABI mapping
description: "How goIsekai Lua/WASM plugin ABI maps onto cppIsekai's handle()/envelope ABI, including host tool gaps that block direct ports."
tags: [plugin, goisekai, abi, lua, reference]
generated: { by: agent/mcp, at: "2026-11-12T05:18:12Z" }
status: stable
---

Ported from ~/tmp/goIsekai (Go experiment, reference only). cppIsekai is the cleaner successor; port logic, never Go code. Goal of the port program is to COMPLETE the host feature set vs goIsekai, and to use a MIX of lua/js/wasm ports so each runtime's gaps become explicit. Prefer the easiest port per site, not uniformity.

goIsekai host regex is also RE2, so host.regex.find/find_all/gmatch -> host.re2.find/match (or inline Lua patterns) translate cleanly.

## Envelope / entry contract
goIsekai: sibling .lua files pre-execute before main.lua (mangafire ships helpers.lua with http_get/normalizeStatus + VRF tables). cppIsekai loads ONLY main.lua via safe_script_file (src/plugin/plugin_load.cpp); siblings are NOT auto-loaded. Ports must merge siblings into main.lua or load them manually via host.fs.read + load(). Single-file merge is preferred.

cppIsekai (Lua): main.json ({name,id,version,website,runtime:'lua',capabilities:['search','detail','chapters'],api:1}) + main.lua with methods.<name>(params) + global handle(req).
handle(req) :: fn -> fn(req.params or {}); every method returns { ok=true, data=... } or { ok=false, error=msg }. req = { api=<kHostApi>, method, params }; params decoded table (mangadex/atsumaru use `title`/`query`/`tags`/`page`, detail/chapters/pages use `id`). Bare global `json.encode/decode` (NOT host.json), 1-based tables, no `os`/`io` except sandboxed io.read/lines.

Method mapping: search_manga(arg)->methods.search (title/query/q, tags, page); get_manga_detail(arg)->methods.detail (id); get_chapter_list(arg)->methods.chapters (id); get_page_list(arg)->methods.pages (id); get_genres()->methods.genres. Canonical output: detail {id,name,cover,status,tags,release_at,synopsis,alt_titles?}; chapters {id,number,title,release_at}; pages = list of image URL strings; search = list of {id,name,cover,...}.

## Host tool mapping (goIsekai -> cppIsekai)
- host.http.get_body/get/post -> host.http.fetch(url, opts); returns (res, err) pair; res.status/res.body; opts: method, body, accept, user_agent, headers, timeout_ms, profile, cookie
- host.json.encode/decode -> bare json.encode/decode (pcall decode)
- host.text.url_encode -> host.url_encode (host.url_decode also exists)
- host.text.trim/strip_html/unescape/normalize_status/date_to_iso/chapter_num -> NO host text lib: inline Lua (gsub + Hinnant civil-epoch); see mangadex/atsumaru plugins
- host.plugin.param("api") -> none; hardcode or config.get (plugin's own config.json)
- host.regex.find/find_all/gmatch (RE2) -> host.re2.find / host.re2.match (RE2, same engine family)
- host.html.* -> host.html.select(h, css), host.html.attr(h, css, attr) (array of strings)
- host.crypto.vrf_sign / sha / md5 / base64 -> NO crypto host tools yet (OKF decisions/host-url-codec: deferred as YAGNI until a plugin needs them); mangafire needs VRF
- host.browser.* -> none; comix blocked
- host.storage.get/set, host.now, host.log.info/error/debug, host.fs.read -> exist (fs confined to plugin dir)

## Portability verdict (checked 2026-11-12)
| Site | Runtime plan | Blockers / notes |
|---|---|---|
| mangafire | lua (first) | BLOCKED until host crypto (vrf_sign or pure-Lua VRF) exists; os.clock must become host.now; helpers.lua merged inline |
| batcave | lua | mostly portable: host.http.fetch + host.re2; html_decode/strip_html/chapter_num inline |
| weebcentral | lua | host.html.find_list_* / find_text / find_attr have no direct equivalents; use host.html.select/attr or RE2; inline normalize_status/chapter_num |
| comik | lua | easiest: host.http.fetch + host.re2.find_all only |
| comix | (dropped by user: "skip comix, ganti ke komix") | host.browser.* missing |
| komix (komix.to) | wasm C++ (port from ~/tmp/goIsekai/examples/plugins/wasm/komix_to_cpp, C++17 + wasi-sdk, 0.3MB) | ABI differs: goIsekai wasm uses env.host_call generic import; cppIsekai host passes concrete imports (now, log_info, fs_read, storage_get, storage_set, http_fetch, html_select, html_attr). Port = rewrite wasm_abi.h host_call layer + RapidJSON usage, keep main.cpp parsers |
| mangapill | wasm | goIsekai only has Go tinygo source (2.8MB), no lua/js version; would need full rewrite, lowest priority |

## Program goals (user, 2026-11-12)
1. mangafire first.
2. Port as a MIX of lua/wasm (and js where natural) so runtime gaps are visible per runtime, not masked by uniformity.
3. Purpose is to complete the host feature set relative to goIsekai; when a port needs a host tool that is missing, either add it to the host (with tests) or do the work inline in the plugin - decide per case, do not silently hand-wave.

## Open questions pending with user
- mangafire VRF: host-side host.crypto.vrf_sign (C++ AES-ish construction as in JS plugin) vs pure-Lua VRF (~100 lines of bit ops; Lua 5.4 integer ops)?
- mangafire VRF tables (VRF_K1..3 / VRF_T1..3) stay in the plugin (site secret), not the host - confirm.
- komix: keep C++ wasm port (needs /opt/wasi-sdk on this machine) vs rewrite in Lua like the rest?

## Golden rules
1. Port site logic, never Go/JS source verbatim; rewrite to the cppIsekai envelope.
2. Verify against real site traffic before trusting selectors; HAR captures in ~/tmp/goIsekai/tmp{,/har}.
3. Reference template for every port: plugins_dev/lua/mangadex/main.lua and plugins_dev/lua/atsumaru/main.lua (handle()/methods/envelope, pcall json.decode, host.http.fetch error pair).
4. OKF decisions/deps-and-plugins: no hand-rolled crypto; md5/sha via stbrumme/hash-library, base64 via glaze.
