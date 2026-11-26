---
type: Finding
title: "harimanga CF challenge: browser fetch + jar/UA bridge"
description: "harimanga.cc CF managed challenge: browser-path fetch + cookies tool + image_ua jar bridge; cf_clearance bound to UA; dua bentuk card Madara."
tags: [harimanga, cloudflare, waf, browser-host, cookies]
generated: { by: agent/mcp, at: "2026-11-26T01:22:45Z" }
status: stable
---

## Symptom (2026-11-25)
`/plugin/harimanga/*` → `upstream status 403`. Direct curl UA Chrome 131 → 403 + `cf-mitigated: challenge`. harimanga.cc zone turned on CF **managed challenge** (JS), bukan TLS-fingerprint challenge: `chrome116` impersonate pun 403, `lightpanda` CLI pun dapat "Just a moment" (bot challenge, resource=worker disabled by default).

## Fix (3 lapis, pola comix)
1. **Data fetches via host browser** — `host.browser.call("goto", url)` (challenge solve sendiri di Chrome xvfb) + `evaluate document.documentElement.outerHTML`. Semua GET harimanga lewat `common.http_req` (single path); chapters POST = goto dulu, lalu **same-origin XHR in-page** (`XMLHttpRequest` + return `JSON.stringify({s:status,b:body})`) — session cookies ikut, tanpa jar minting.
2. **`cookies` browser tool** — `host.browser.call("cookies", {domain})` → Netscape jar line (httpOnly `cf_clearance` included, dari CDP `Network.getAllCookies` — browser-level `Storage.getCookies` cuma 1 cookie, page-session lebih lengkap). Seed: `scripts/harvest_cookies.py <cdp_port> harimanga.cc data/plugin-state/harimanga.cookies.txt` (port dari log `DevTools listening on ws://127.0.0.1:PORT`).
3. **Image path jar+UA bridge (host)** — `makeCurlFetcher` feed `data/plugin-state/<id>.cookies.txt` ke `fetch::Request.cookie_jar` + `image_ua` manifest → UA. **cf_clearance terikat ke UA token yang mint** (jar + UA 131 = 403; jar + UA 158 = 200): UA harus = Chrome yang solve (xvfb chromium 158).

## Gotchas
- `image_engine: chrome116` + `image_ua` di manifest; tanpa `image_ua` curl-impersonate UA (116) mismatch → 403.
- cf_clearance expired / chrome update → re-mint: satu kali call harimanga (goto mint), lalu harvest lagi. `COOKIEJAR` write-back: fetch sukses update jar otomatis.
- Eval result JS: pakai `JSON.stringify({..})` + `pcall(json.decode,..)` — string-concat separator via long-string [[ ]] = literal backslash trap; `x.status` dari CDP = number.
- Madara DUA bentuk card: search `.c-tabs-item__content` (h3) vs genre archive `.page-item-detail` (h3 class h5 + whitespace sebelum `<a>`) — parser jalanin keduanya, title regex `<h[1-6]>\s*<a`. Genre archive HREF relative (`/manga-genre/`) — jangan pin domain.
- genre_map = shared table di common.lua (genres mutasi referensi yang sama); rebind local tidak propagate — bug tag browse 0 hasil sebelum fix.
- Restart daemon after host rebuild: `pkill -f release/cppIsekai` + relaunch `setsid ... &`.

generated: { by: "kit-agent", at: "2026-11-26" }

# Related Concepts
- [comix WAF turnstile unsolvable in lightpanda headless; session ops notes](browser-session-waf-2026-11-17.md): harimanga repeats the comix WAF pattern with the app-local chrome; adds cookie/UA bridge for image path
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): cookies tool + image_ua extend the host surface used by the plugin system
