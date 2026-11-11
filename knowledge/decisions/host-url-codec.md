---
type: Decision
title: "host.url_encode/url_decode: percent-codec host tool (no crypto yet)"
description: "host.url_encode / host.url_decode added to the Lua plugin ABI: RFC 3986 percent-codec with '+'->space decode; no crypto added (YAGNI — real crypto host tools only when a plugin needs them)."
tags: [plugin, abi, lua, url, codec, host-tools]
generated: { by: agent/mcp, at: "2026-11-11T11:54:28Z" }
status: stable
---

## Decision (2026-11-11, commit d988f64)

Query-component codec exported to plugins as `host.url_encode(s)` / `host.url_decode(s)` (Lua ABI, `src/plugin/lua/lua_runtime.cpp`, registered next to `host.now`).

- encode: RFC 3986 percent-encoding — unreserved `ALPHA DIGIT '-' '.' '_' '~'` pass through, everything else `%XX` (uppercase hex). Single-byte safe on raw binary.
- decode: `%XX` -> byte; `+` -> space (query-string convention). Malformed `%` tails pass through verbatim (no throw).
- Not crypto. The user framing was "crypto for encoding/decoding url if needed" — atsumaru's Typesense query building needed only a percent-codec, so no hash/cipher host tools were added. Add real crypto host tools (md5/hmac/base64) only when a plugin actually needs them.
- Motivation: atsumaru search builds `filter_by`/`query_by` with commas, brackets, spaces; hand-rolled gsub escaping in the mangadex plugin is exactly the class of bug this centralizes.

Semantics note: this is a component codec, not a full-URL encoder — plugins encode each query value, not whole URLs.
