---
type: Finding
title: "harimanga CF challenge: browser fetch + jar/UA bridge"
description: "harimanga.cc CF managed challenge: browser fetch + challenge-wait, eksternal jar seed, image_ua bridge; cf_clearance bound to UA."
tags: [harimanga, cloudflare, waf, browser-host, cookies]
generated: { by: agent/mcp, at: "2026-11-26T01:27:25Z" }
status: stable
---

## Symptom (2026-11-25)
`/plugin/harimanga/*` → `upstream status 403`. Direct curl UA Chrome 131 → 403 + `cf-mitigated: challenge`. harimanga.cc zone turned on CF **managed challenge** (JS), bukan TLS-fingerprint challenge: `chrome116` impersonate pun 403, `lightpanda` CLI pun dapat "Just a moment" (bot challenge, resource=worker disabled by default).

## Fix
1. **Data fetches via host browser** — `common.http_req`: `goto` (challenge solve sendiri di Chrome xvfb) + **`wait` title !="Just a moment"** (WAJIB: nav balik sebelum challenge selesai → evaluate nangkep interstitial → 0 item diam-diam; race cold-start) + `evaluate outerHTML`. Semua GET lewat sini; chapters POST = goto+wait, lalu **same-origin XHR** (`JSON.stringify({s:status,b:body})`) — session cookies ikut.
2. **Jar seed eksternal** — `scripts/harvest_cookies.py <cdp_port> harimanga.cc data/plugin-state/harimanga.cookies.txt` via CDP Chrome langsung (`Network.getAllCookies`, httpOnly cf_clearance; browser-level `Storage.getCookies` cuma 1 cookie). Port dari log `DevTools listening on ws://127.0.0.1:PORT`.
3. **Image path jar+UA bridge (host)** — `makeCurlFetcher` feed `data/plugin-state/<id>.cookies.txt` + `image_ua` manifest → UA. **cf_clearance terikat UA token yang mint** (jar+UA131=403, jar+UA158=200).

## Catatan host
- Tool browser `cookies` sempat ditambah lalu **dihapus lagi** (zero caller; seed cukup via script CDP eksternal) — jangan dibikin ulang tanpa konsumen.
- `image_engine: chrome116` + `image_ua` di manifest; tanpa `image_ua` UA impersonate (116) mismatch → 403.
- cf_clearance expired / chrome update → re-mint: call harimanga sekali (goto mint) + harvest ulang; `COOKIEJAR` write-back otomatis di fetch sukses.
- Eval JS: `JSON.stringify({..})` + `pcall(json.decode)`; `x.status` = number.
- Madara DUA card shape: search `.c-tabs-item__content` vs archive `.page-item-detail` (title regex `<h[1-6]>\s*<a`, href relative) — parser jalanin keduanya.
- genre_map = shared table di common.lua; rebind local tidak propagate (bug tag browse 0 hasil).
- Restart daemon: `pkill -f release/cppIsekai` + `setsid ... &`; cek `pgrep` — nohup subshell kadang mati.

generated: { by: "kit-agent", at: "2026-11-26" }

# Related Concepts
- [comix WAF turnstile unsolvable in lightpanda headless; session ops notes](browser-session-waf-2026-11-17.md): harimanga repeats the comix WAF pattern; challenge-wait + external jar seed, no daemon cookies tool
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): image_ua manifest field + jar bridge extend the host plugin surface
