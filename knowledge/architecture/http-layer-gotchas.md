---
type: Fact
title: "src/http compile gotchas: global-scope helpers in api_ctx.hpp; crow::response is move-only"
description: "Compile + runtime traps in src/http: api_ctx.hpp namespace qualification, crow::response move-only, and crow::response move assignment dropping the `compressed` flag (double-compression of precompressed .br assets — fix via out-param route signature)."
tags: [crow, api_ctx, compile, namespaces, http]
generated: { by: agent/mcp, at: "2026-11-12T03:12:13Z" }
status: stable
---

Repeat traps in src/http:

1. Helpers defined below the namespace close of api_ctx.hpp must qualify http::/http::api:: names.
2. crow::response locals must stay non-const to be returned (copy ctor deleted).
3. crow::response move assignment (`operator=(response&&)`) moves body/code/headers/completed_/file_info ONLY — the `compressed` flag is silently dropped back to default `true` when a handler returns a response by value. With `app.use_compression(GZIP)` this means any handler that sets `res.compressed=false` on a return-value response still gets gzip'd when the client advertises gzip. Fatal for precompressed bodies: br_asset served .br bytes, Crow gzipped them, browsers decoded gzip and got raw brotli as JS → "Uncaught SyntaxError" for tailwind/htmx/alpine → whole page unstyled (2026-11-12). Hidden for days by Cache-Control: immutable; surfaced on hard refresh. Fix: serve such routes with the out-param signature `(const crow::request&, crow::response&)` — no move, flag survives. Verify with: curl -H 'Accept-Encoding: gzip, deflate, br' -D - <url> and check Content-Encoding matches the actual body (1f8b=gzip, brotli has no magic — decode with brotli -dc + node --check for JS).
