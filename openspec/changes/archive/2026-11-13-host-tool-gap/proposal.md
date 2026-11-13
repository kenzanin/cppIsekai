# Host-Tool Gap Closure

## Why
goIsekai Lua plugins use 17 host tools that cppIsekai does not expose. Porting goIsekai sites currently requires rewriting helpers manually per site (trim, unescape, strip_html, chapter_num, normalize_status, date_to_iso, regex.find_all, http.get_json, etc.) — 185 call sites across 38 sites. This change closes the gap so porting becomes copy-paste + rename.

## What Changes
- **P1**: `host.plugin.param` (42 sites), `host.http.fetch_body` (99 sites) — 20 lines, std lib
- **P2**: `host.text.normalize_status/chapter_num/date_to_iso` (75 sites) — 150 lines, std + RE2
- **P3**: `host.text.trim/unescape/strip_html` (134 sites) — 100 lines, lexbor + utf8proc
- **P4**: `host.re2.find_all/gmatch/replace/quote/find_index` (72+ sites) — 50 lines, RE2
- **P5**: `host.http.get_json/post_json` (99 sites) — 15 lines, glaze
- **P6**: `host.codecs.base64_encode/decode` + `host.codecs.hex_encode/decode` (12 sites) — 30 lines, std
- **P7**: `host.crypto.sha256_hex/md5_hex/hmac_sha256_hex/aes_gcm_decrypt` (5+3 sites) — 100 lines, **hash-library + tiny-aes-c**
- **P8**: `host.browser.*` — **SKIPPED** (CDP cookie duration too short, not effective)
- **P9**: `host.html.xpath_*` — **IMPLEMENT** (pugixml v1.16 di xrepo, light-weight, XPath support)

## Scope
- **In scope**: P1-P7 + P9 (host helper functions)
- **Out of scope**: P8 (browser), sqlean (overlap with existing deps)

## Design Decisions (D1a-D7)
- **D1a** P1: `host.plugin.param` = read `main.json` `params` at load; `host.http.fetch_body` = wrapper over `host.http.fetch` that returns `res.body` or nil on non-200
- **D1b** P2: `host.text.normalize_status` = port `pluginutil/status.go` map verbatim; `host.text.chapter_num` = port `extract.go` regexes to RE2; `host.text.date_to_iso` = port `dates.go` `dateLayouts` table + `std::chrono` parse + relative phrase map
- **D1c** P3: `host.text.trim` = utf8proc-aware trim; `host.text.unescape` = lexbor entity tables; `host.text.strip_html` = lexbor walk + text nodes + trim + unescape
- **D1d** P4: `host.re2.find_all` = `RE2::FindN` loop; `host.re2.gmatch` = sol2 closure over vector (pattern already in `lua_runtime.cpp:101`); `host.re2.replace` = `RE2::ReplaceAll` + `RE2::GlobalReplace`; `host.re2.quote` = manual escape; `host.re2.find_index` = `RE2::Find` + position loop
- **D1e** P5: `host.http.get_json` = `fetch` + `glz::read` + return table-or-nil; `host.http.post_json` = same with method POST
- **D1f** P6: `host.codecs.base64_encode/decode` + `host.codecs.hex_encode/decode` = std `std::format` + manual base64/hex
- **D1g** P7: **hash-library 2021.09.29 + tiny-aes-c 2021.12.22** (xrepo) for `host.crypto.sha256_hex/md5_hex/hmac_sha256_hex/aes_gcm_decrypt`. Both libs tiny, no external deps, avoids OpenSSL overkill (sandbox install pain)
- **D1h** P8: Skipped — CDP cookie duration too short, curl-impersonate already covers request fingerprint
- **D1i** P9: **IMPLEMENT** — pugixml v1.16 di xrepo, light-weight XML parser with XPath support. `host.html.xpath(expr)` = lexbor parse → XML serialize → pugixml `select_nodes` → return text/attr nodes

## Risks
- P7 hash-library + tiny-aes-c adds 2 tiny deps (both xrepo, no external deps)
- P2 `date_to_iso` human date parsing is fragile (site formats vary)
- P3 lexbor entity tables may miss rare entities
