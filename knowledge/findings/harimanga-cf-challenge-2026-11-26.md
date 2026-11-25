---
type: Finding
title: "harimanga CF challenge: browser fetch + jar/UA bridge"
description: "harimanga.cc CF managed challenge: browser-path fetch + cookies tool + image_ua jar bridge; cf_clearance bound to UA."
tags: [harimanga, cloudflare, waf, browser-host, cookies]
generated: { by: agent/mcp, at: "2026-11-25T18:20:18Z" }
status: stable
---

## Symptom (2026-11-25)
`/plugin/harimanga/*` → `upstream status 403`. Direct curl UA Chrome 131 → 403 + `cf-mitigated: challenge`. harimanga.cc zone turned on CF **managed challenge** (JS), bukan TLS-fingerprint challenge: `chrome116` impersonate pun 403, `lightpanda` CLI pun dapat "Just a moment" (bot challenge, resource=worker disabled by default).

## Fix (3 lapis, pola comix)
1. **Data fetches via host browser** — `host.browser.call("goto", url)` (challenge solve itself di Chrome xvfb) + `evaluate document.documentElement.outerHTML`. Semua GET harimanga (search/detail/genres/pages) lewat jalur ini; chapters POST = goto dulu, lalu **same-origin XHR in-page** (`XMLHttpRequest` + return `JSON.stringify({s:status,b:body})`) — session cookies ikut, tanpa jar minting.
2. **`cookies` browser tool (baru)** — `host.browser.call("cookies", {domain})` → Netscape jar line (httpOnly `cf_clearance` included, dari CDP `Network.getAllCookies` — browser-level `Storage.getCookies` cuma 1 cookie, page-session lebih lengkap). Seed: `python3 tmp/harvest_cookies.py <cdp_port> harimanga.cc data/plugin-state/harimanga.cookies.txt` (port dari log `DevTools listening on ws://127.0.0.1:PORT`).
3. **Image path jar+UA bridge (host)** — `makeCurlFetcher` feed `data/plugin-state/<id>.cookies.txt` ke `fetch::Request.cookie_jar` + `image_ua` manifest → UA. **cf_clearance terikat ke UA token yang mint** (jar + UA 131 = 403; jar + UA 158 = 200): UA harus = Chrome yang solve (xvfb chromium 158).

## Gotchas
- `image_engine: chrome116` + `image_ua` di manifest; tanpa `image_ua` curl-impersonate UA (116) mismatch → 403.
- cf_clearance expired / chrome update → re-mint: satu kali `goto` harimanga via daemon (semua call harimanga melakukannya), lalu harvest lagi. `COOKIEJAR` write-back: fetch sukses update jar otomatis.
- Eval result JS `"\n"` via long-string [[ ]] = literal backslash → pakai `JSON.stringify({..})` + `pcall(json.decode,..)`, jangan string-concat separator.
- `x.status` dari CDP evaluate = number → `tonumber`, bukan string compare.
- Madara genre href relative (`/manga-genre/`) di theme ini — regex jangan pin domain.
- Restart daemon after host rebuild: `pkill -f release/cppIsekai` + relaunch `setsid ... &` (nohup dalam subshell kadang mati saat sesi shell close).

generated: { by: "kit-agent", at: "2026-11-26" }
