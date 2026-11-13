---
type: reference
title: host-tool gap analysis (goIsekai → cppIsekai)
description: "goIsekai→cppIsekai host-tool gap map (P1–P9): ALL phases landed, change ARCHIVED 2026-11-13, main spec openspec/specs/host-tools/spec.md (7 requirements); P8 skipped (CDP)."
tags: [host-tools, gap-analysis, goisekai-porting, priority]
generated: { by: agent/mcp, at: "2026-11-13T11:23:44Z" }
status: draft
---

# Host-Tool Gap Analysis (goIsekai → cppIsekai)

## Summary
Mapping lengkap host-tools yang goIsekai Lua plugin pakai (38 sites, ~1400 call sites) vs yang cppIsekai host binding expose. Identifikasi gap + prioritas + library yang bisa dipake.

## Implementation status (host-tool-gap openspec change)
| Phase | Status | Commit |
|---|---|---|
| P1 plugin.param + fetch_body | ✅ done | 7e5c762 |
| P2 normalize_status/chapter_num/date_to_iso (date v3.0.4) | ✅ done | b5a2df8 |
| P3 trim/unescape/strip_html (utf8proc+lexbor) | ✅ done | e7df3c1 |
| P4 re2 find_all/gmatch/replace/quote/find_index ($N→\N in shared core) | ✅ done | a672c59 |
| P5 http.get_json/post_json (glaze) | ✅ done | 1032ed3 |
| P6 codecs base64/hex (reuse vrf_b64_decode) | ✅ done | 3e2d6cd |
| P7 crypto sha256/md5/hmac/aes_gcm (hash-library + tiny-aes256 local target) | ✅ done | 45a1f5f |
| P8 browser.* | ⏭️ skipped — CDP cookie duration too short | — |
| P9 html.xpath (pugixml v1.16) | ✅ done | e56415c |
| Phase 9 docs (knowledge + plugin-port parity sync) | ✅ done | 780dc73, 87bc2c2 |

ARCHIVED 2026-11-13 as `openspec/changes/archive/2026-11-13-host-tool-gap/`; main spec created at `openspec/specs/host-tools/spec.md` (7 requirements merged by `openspec archive`). Archive commit `679f51f`.

All phases gated: zero-warning build + 17/17 tests. Bindings are thin marshals over one shared core per rule (host helpers written once, linked into sol2/quickjs-ng/wasm3).

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

## cppIsekai current (post P1–P7)
- **Lua** (`lua_runtime.cpp`): host.json, host.re2.{find,match,find_all,gmatch,replace,quote,find_index}, host.html.{select,attr}, host.http.{fetch,fetch_body,get_json,post_json}, host.storage, host.log, host.fs + require(), host.url_encode/decode, host.now, host.io, host.plugin.param, host.codecs.{base64,hex}_encode/decode, host.text.{trim,unescape,strip_html,normalize_status,chapter_num,date_to_iso}, host.crypto.{vrf_sign,sha256_hex,md5_hex,hmac_sha256_hex,aes_gcm_decrypt}
- **JS** (`js_bindings.cpp`): same surface (QuickJS builtin json), plus host.config.get
- **WASM** (`wasm_imports.cpp`): full import set incl. text/codecs/crypto/http-json (text-in/text-out convention, kNotFound on failure; get_json returns JSON text gated on validity — guests parse themselves)

## Gap Table (final decisions)

| # | Gap | Call sites | Status | Lib |
|---|---|---|---|---|
| P1 | `host.plugin.param` + `host.http.fetch_body` | 42 + 99 | ✅ | std |
| P2 | `host.text.normalize_status/chapter_num/date_to_iso` | 75 | ✅ | std + RE2 + date v3.0.4 |
| P3 | `host.text.trim/unescape/strip_html` | 134 | ✅ | lexbor + utf8proc |
| P4 | `host.re2.find_all/gmatch/replace/quote/find_index` | 72+ | ✅ | RE2 |
| P5 | `host.http.get_json/post_json` | 99 | ✅ | glaze |
| P6 | `host.codecs.*` (base64/hex; base64url covered by vrf_b64url) | 12 | ✅ | std |
| P7 | `host.crypto.*` (sha256/md5/hmac/aes_gcm) | 5+3 | ✅ | hash-library + tiny-aes256 (local target; xrepo recipe is AES128-only) |
| P8 | `host.browser.*` | 3 | ⏭️ skipped | CDP cookie duration too short; curl-impersonate covers request-fingerprint half |
| P9 | `host.html.xpath` | 6 | ✅ | pugixml v1.16 (lexbor parse → HTML5 serialize + void-self-close + 10-entity fold → select_nodes) |

## Fetch repeats
- `kaliscan` pake 5 fetch per search (walk pages) → cache LRU per-query
- `kaliscan` pake 2 detail fetch per manga → dedup URL map
- `comico` token POST → `host.storage` caching
- **Ideal**: host-level URL dedup cache (sub-second TTL, hash map) — not yet implemented

## Library (final)
- **lexbor 3.0.0** — strip_html, unescape, find_*, list_*
- **RE2 2025.11.05** — find_all, gmatch, replace, quote, find_index, chapter_num
- **glaze 7.9.0** — JSON decode/encode (get_json/post_json)
- **utf8proc 2.11.3** — unicode-aware trim/NFKD
- **date v3.0.4** — date_to_iso layouts
- **hash-library 2021.09.29** — sha256/md5/hmac
- **tiny-aes-c source (vendored third_party/tiny-aes, local target tiny-aes256, AES256=1)** — AES-256-GCM decrypt; xrepo recipe builds AES128-only with no config hook; GCM (CTR+GHASH+tag) hand-rolled in host_crypto.hpp, NIST SP 800-38D KATs
- **pugixml 1.16** — xpath (P9)

## Manual helper duplikasi di goIsekai
- `fanfox/util.lua`, `mangahere/util.lua`, `mangatown/util.lua`, `mangafreak/util.lua`, `mangadex/util.lua`, `mangafire/helpers.lua` — **6 copy of ~40-line block** (trim(unescape(x)), get_json, image_headers, normalizeStatus) — now eliminable via host tools above

## Final decisions (supersede earlier escalations)
- P7: Option A — hash-library + tiny-aes (NOT OpenSSL; sandbox install too heavy for a few functions; NOT libsodium — ChaCha20-Poly1305 ≠ AES-GCM)
- P8: skipped (CDP cookie duration too short)
- P9: pugixml v1.16 (earlier "no cheap lib" escalation resolved)

# Related Concepts
- [goIsekai -> cppIsekai plugin porting matrix](plugin-porting-matrix-goIsekai.md): Gap analysis menginformasikan prioritas porting plugin (P1-P9) — matrix menentukan site mana yang di-port, analysis menentukan apa yang perlu ditambal di host dulu.
- [openspec delta spec format (archive gate)](openspec-delta-spec-format.md): The delta format gate blocked this change's first archive attempt (2026-11-13).
- [Daemon run & verify (run-cppIsekai.sh)](daemon-run-verify.md): P1–P9 host tools verified live through the daemon restart documented there.
