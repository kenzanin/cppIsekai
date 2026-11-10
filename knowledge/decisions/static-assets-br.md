---
type: Decision
title: "Static assets: vendored, precompressed brotli-only via explicit routes"
description: "Vendored JS ships as precomputed .br via explicit crow routes with year-immutable caching; CDN runtime and plain siblings dropped, brotli treated as universal."
tags: [http, static-assets, brotli, caching, offline-first]
generated: { by: agent/mcp, at: "2026-11-10T16:39:31Z" }
status: stable
---

## Contract

- Templates load `/static/{tailwind,htmx,alpine}.min.js`; each is an explicit Crow route (`http::register_static` in `src/http/http_pages.cpp`, helper `br_asset`).
- Handler serves the precomputed `<file>.br` sibling when `Accept-Encoding` contains `br`; if the plain sibling is missing it serves `.br` with `Content-Encoding: br` regardless (plain `htmx.min.js`/`alpine.min.js` deleted; plain tailwind never shipped — brotli treated as universal in modern browsers).
- `Cache-Control: public, max-age=31536000, immutable` on all three; Crow's built-in `/static` handler remains the fallback for anything unhandled.
- Tailwind is the vendored play-CDN runtime (~102 KB br) — the cdn.tailwindcss.com dependency is dropped; the daemon renders correctly with no internet.

## Why

Local-first daemon: zero CDN round-trips, ~5x smaller JS payloads, year-immutable caching. Plugin-fetch TLS constraints are a separate concern (see fetch-profile-gate line of work).

Commit 49e39ff on main; browser-verified headers (`Content-Encoding: br`, immutable) 2026-11-10.
