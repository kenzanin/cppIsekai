---
type: reference
title: host-tool gap analysis (goIsekai → cppIsekai)
tags: [host-tools, gap-analysis, goisekai-porting, priority]
generated: { by: agent/mcp, at: "2026-11-13T04:22:09Z" }
status: draft
---

# Host-Tool Gap Analysis (goIsekai → cppIsekai)

## Summary
Mapping lengkap host-tools yang goIsekai Lua plugin pakai (38 sites, ~1400 call sites) vs yang cppIsekai host binding expose. Identifikasi gap + prioritas + library yang bisa dipake.

## Inventory goIsekai (dari `host_surface.go` + grep 38 site)
- `json`: encode (425), decode (181)
- `regex`: find (280), gmatch (72), replace (21), find_index (20), match (19), find_all (7), quote (5)
- `text`: url_encode (96), trim (55), strip_html (38), unescape (29), normalize_status (29), date_to_iso (23), chapter_num (23), html_decode (11), json_blob (4), url_decode (1)
- `http`: get_body (99), get (15), post_body (7), post (4)
- `html`: find_list_attr (27+19), find_attr (18+12), find_list_text (16+11), find_text (15+9), parse (10+6), xpath_* (6)
- `crypto`: aes_gcm_decrypt (5), vrf_sign (4), sha256_hex (2), md5_hex (1)
- `browser`: fetch (3), evaluate (2), evaluate_with_init (2)
- `plugin`: param (42)
- `encoding`: base64_url_encode (1)

## cppIsekai current
- **Lua** (`lua_runtime.cpp`): `host.json.{encode,decode}`, `host.re2.{find,match}`, `host.html.{select,attr}`, `host.http.fetch(url,opts)→{status,body,headers}`, `host.storage.{get,set}`, `host.log.{info,error,debug}`, `host.fs.read` + `require()`, `host.url_encode/url_decode`, `host.now`, `host.io`, `host.crypto.vrf_sign`
- **JS** (`js_bindings.cpp`): same minus `json.*` (QuickJS builtin), plus `host.config.get`
- **WASM** (`wasm_imports.cpp`): `env.{now,log_info,fs_read,storage_get,storage_set,http_fetch,re2_match,re2_find,html_select,html_attr,url_encode,url_decode}` — no crypto, no config

## Gap Table (priority order)

| # | Gap | Call sites | Work | Lib |
|---|---|---|---|---|
| P1 | `host.plugin.param` + `host.http.fetch_body` | 42 + 99 | 20 baris | std |
| P2 | `host.text.normalize_status/chapter_num/date_to_iso` | 75 | 150 baris | std + RE2 |
| P3 | `host.text.trim/unescape/strip_html` | 134 | 100 baris | lexbor + utf8proc |
| P4 | `host.re2.find_all/gmatch/replace/quote/find_index` | 72+ | 50 baris | RE2 |
| P5 | `host.http.get_json/post_json` | 99 | 15 baris | glaze |
| P6 | `host.codecs.*` (base64/hex) | 12 | 30 baris | std/zlib |
| P7 | `host.crypto.*` (sha256/md5/hmac/aes_gcm) | 5+3 | 100 baris | **OpenSSL dep** |
| P8 | `host.browser.*` | 3 | strategic | escalate ke oracle |
| P9 | `host.html.xpath_*` | 6 | real gap, no lib | escalate ke oracle |

## Fetch repeats
- `kaliscan` pake 5 fetch per search (walk pages) → cache LRU per-query
- `kaliscan` pake 2 detail fetch per manga → dedup URL map
- `comico` token POST → `host.storage` caching
- **Ideal**: host-level URL dedup cache (sub-second TTL, hash map)

## Library
- **lexbor 3.0.0** — strip_html, unescape, find_*, list_*
- **RE2 2025.11.05** — find_all, gmatch, replace, quote, find_index, chapter_num, json_blob
- **glaze 7.9.0** — JSON decode/encode
- **utf8proc 2.11.3** — unicode-aware trim
- **OpenSSL** (sistem Arch) — P7 (AES-GCM, sha256, md5, hmac)
- **zlib** (xrepo) — base64 encode/decode

## Manual helper duplikasi di goIsekai
- `fanfox/util.lua`, `mangahere/util.lua`, `mangatown/util.lua`, `mangafreak/util.lua`, `mangadex/util.lua`, `mangafire/helpers.lua` — **6 copy of ~40-line block** (trim(unescape(x)), get_json, image_headers, normalizeStatus)
- `mangafire/helpers.lua` header: *"Copy file into any plugin folder unchanged… escaping status vocabulary live in host"*

## Escalate ke oracle
- P7 (OpenSSL vs libsodium choice)
- P8 (browser.* — CDP route ada tapi bukan plugin tool yet)
- P9 (xpath — lexbor gak support, no lib C++ murah)

# Related Concepts
- [goIsekai -> cppIsekai plugin porting matrix](plugin-porting-matrix-goIsekai.md): Gap analysis menginformasikan prioritas porting plugin (P1-P9) — matrix menentukan site mana yang di-port, analysis menentukan apa yang perlu ditambal di host dulu.
