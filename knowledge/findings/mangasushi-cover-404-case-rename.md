---
type: Bug
title: "mangasushi: 2 covers 404, upstream case-renamed files, daemon emits stale lowercase URLs"
description: "Seisan/LONELY covers 404 on /cache/img: upstream case-renamed files; api_cache, fetch-stack lowercasing and stale installed plugin copy (data/plugins == plugins_dev, byte-identical) all ruled out; only CF edge variant per TLS fingerprint (chrome116) remains unproven."
tags: [mangasushi, cache, cover, 404, image-proxy, debug]
generated: { by: agent/mcp, at: "2026-11-16T06:21:20Z" }
status: stable
---

Two of nine covers 404 on /cache/img for `search?plugin=mangasushi&q=isekai` (reqids 3638/3639): `.../2022/08/seisan-193x278.jpg`, `.../2022/01/lonely-193x278.jpg`.

Proven:
- Upstream 404s those lowercase paths directly (urllib); live HTML (search cards + detail summary_image, 3 fetch variants) now serves capitalized `Seisan-`/`LONELY-193x278.jpg`. Upstream renamed files with case changes (WordPress Madara `-193x278` thumbnails).
- Capitalized URLs pass the image proxy: 200, ~15 KB, image/jxl. Proxy + disk cache healthy.
- Daemon plugin output stays lowercase even after `POST /plugins/cache-clear {"plugin":"mangasushi"}` (cleared 4; envelopes only; fresh call had no `cached:true`) -> route api_cache/stale layer ruled out.
- No URL lowercasing in the fetch stack: `src/fetch/fetch.cpp:46` lowercases response-header names only; host_text helpers do status/unit normalization + case-insensitive rel match; plugin copies `data-src` verbatim (`plugins_dev/lua/mangasushi/main.lua`).
- Stale installed plugin copy RULED OUT (2026-11-16): daemon loads from `data/plugins` (registry default `plugins-dir` = `data/plugins`; `data/cppisekai.ini` has no `[paths]`/`[plugin]` plugins override), and `data/plugins/mangasushi/{main.lua,genres.lua,main.json}` are byte-identical to `plugins_dev/lua/mangasushi/` (diff -q clean). Repo-local search found no other mangasushi main.lua.

Open root cause, one suspect left:
1. Cloudflare edge variant per TLS fingerprint (chrome116 via curl-impersonate) serving older HTML than urllib sees. Fetch the search page through the daemon's own fetch path (`fetch::sync`, chrome116) and diff the cover URLs against a plain urllib/OpenSSL fetch.

Ruled out:
- api_cache / stale layer (cache-clear + fresh call still lowercase).
- Fetch-stack lowercasing.
- Stale installed plugin copy (identical trees, daemon reads data/plugins).

User-facing: no code fix yet; covers self-heal once lowercase paths resolve upstream or URLs come back capitalized. Do NOT add a case-folding fallback until suspect 1 is proven. Daemon was not running at time of last check (no build/linux/.../cppIsekai process) — live re-test pending.

# Related Concepts
- [cover prefetch: dedicated worker, cache-first, library + search hooks](../architecture/cover-prefetch-worker.md): Broken covers surfaced through the /cache/img?cover=1 CoverCtx serving path; diagnosis of upstream 404s for stale lowercase URLs.
