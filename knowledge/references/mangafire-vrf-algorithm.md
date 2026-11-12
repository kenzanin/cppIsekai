---
type: Reference
title: Mangafire VRF algorithm (port target for host.crypto.vrf_sign)
description: Mangafire VRF signing algorithm as implemented in goIsekai pluginutil.VRFSign — port byte-for-byte into host crypto host tool.
tags: [plugin, mangafire, vrf, crypto]
generated: { by: agent/mcp, at: "2026-11-12T05:25:39Z" }
status: stable
---

Source: ~/tmp/goIsekai/internal/pluginutil/vrf_sign.go (port behavior, not Go code verbatim structure).

## Wire contract (host.crypto.vrf_sign(api_path, params_table, stages))
- signStr = apiPath with a leading '/api' trimmed, then if params non-empty: '?' + sorted-by-key 'k=v' pairs joined with '&' (raw values, NOT urlencoded — caller signs already-raw values; VRFSign does no escaping)
- stages: [{ iv: byte, key: <base64 std b64>, tbl: <base64 std b64> }...] supplied BY THE PLUGIN as data (so key rotation is a plugin edit, no host rebuild)
- base64 decode key/tbl with StdEncoding (Go base64.StdEncoding)
- data = bytes(signStr); for each stage: out[i] = tbl[(data[i] ^ key[i % len(key)] ^ prev) & 0xFF], prev seeded with stage.IV, prev=out[i] each step
- final result = RawURLEncoding (base64url, no padding) of the bytes

## Plugin-side facts (from goIsekai mangafire)
- VRF tables VRF_K1..K3/VRF_T1..T3 stay in the plugin (site secret), not the host
- VRF_STAGES = { {iv=0x5A,key=VRF_K1,tbl=VRF_T1}, {iv=0x35,...T2}, {iv=0xBA,...T3} }
- API calls carry the vrf param last; array params are sent as 'genres_in[]=13' repeated raw in the URL but signed as 'genres_in[0]=13&genres_in[1]=79' (indexed form covers the signature)
- Empty/absent keyword must be omitted from params entirely (empty param changes the signed parameter set; site answers 403)
- Image CDN (img-r1.2xstorage.com) needs Referer: https://mangafire.to/

## Test vectors
goIsekai has internal/pluginutil/vrf_sign_test.go with vectors; port those into tests/test_crypto.cpp style checks when implementing in C++.
