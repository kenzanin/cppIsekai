---
type: Finding
title: "harimanga CF challenge: browser fetch + jar/UA bridge"
description: "harimanga.cc CF managed challenge: browser fetch + challenge-wait, eksternal jar seed, image_ua bridge; cf_clearance bound to UA."
tags: [harimanga, cloudflare, waf, browser-host, cookies]
generated: { by: agent/mcp, at: "2026-11-26T11:51:34Z" }
status: stable
---

## Symptom (2026-11-25)
`/plugin/harimanga/*` → `upstream status 403`. Direct curl UA Chrome 131 → 403 + `cf-mitigated: challenge`. harimanga.cc zone turned on CF **managed challenge** (JS), bukan TLS-fingerprint challenge: `chrome116` impersonate pun 403, `lightpanda` CLI pun dapat "Just a moment" (bot challenge, resource=worker disabled by default).

## Fix
1. **Data fetches via host browser** — `common.http_req`: `goto` (challenge solve sendiri di Chrome xvfb) + **`wait` title !="Just a moment"** (WAJIB: nav balik sebelum challenge selesai → evaluate nangkep interstitial → 0 item diam-diam; race cold-start) + `evaluate outerHTML`. Semua GET lewat sini; chapters POST = goto+wait, lalu **same-origin XHR** (`JSON.stringify({s:status,b:body})`) — session cookies ikut.
2. **Jar seed in-band** — plugin memanggil `host.browser.call("seed-jar", {plugin=..., domain=...})` sekali per sesi setelah goto yang lolos (tool di `src/browser/chrome_call.cpp`: `Network.getAllCookies` → Netscape jar di path `plugin::cookie_jar_path`, httpOnly `cf_clearance` ikut). Self-healing: tidak ada langkah manual, tidak ada script eksternal.
3. **Image path jar+UA bridge (host)** — `makeCurlFetcher` feed jar plugin + `image_ua` manifest → UA lewat satu accessor `Host::image_fetch(id)` ({engine, ua}, satu lock). **cf_clearance terikat UA token yang mint** (jar+UA131=403, jar+UA158=200).

## Catatan host
- `plugin::cookie_jar_path(id)` (src/plugin/http_client.hpp) = satu-satunya pemilik layout jar; jangan tulis literal `data/plugin-state` di tempat lain.
- `image_engine: chrome116` + `image_ua` di manifest; tanpa `image_ua` UA impersonate (116) mismatch → 403.
- cf_clearance expired / chrome update → re-mint otomatis: satu call harimanga (goto mint + seed-jar); `COOKIEJAR` write-back di fetch sukses ikut memperbarui.
- Eval JS: `JSON.stringify({..})` + `pcall(json.decode)`; `x.status` = number.
- Madara DUA card shape: search `.c-tabs-item__content` vs archive `.page-item-detail` (title regex `<h[1-6]>\s*<a`, href relative) — parser jalanin keduanya.
- genre_map = shared table di common.lua; rebind local tidak propagate (bug tag browse 0 hasil).
- Restart daemon: `pkill -f release/cppIsekai` + `(setsid nohup ./build/... >>log 2>&1 </dev/null &)`; tanpa `nohup` proses ikut mati saat shell penutup keluar.

generated: { by: "kit-agent", at: "2026-11-26" }
