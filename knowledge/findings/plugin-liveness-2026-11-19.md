---
type: Finding
title: "Plugin liveness scan: only kaliscan.me and anisascans.in are globally dead; most peers fail locally on ISP DNS interception"
description: "Full 36-plugin scan through /plugin/<id>/search plus DoH and direct-IP TLS probes. Two domains are gone (NXDOMAIN / no authoritative NS) and were unlinked from data/plugins; the rest are alive globally and only fail on this machine's poisoned ISP resolver."
tags: [plugins, dns, liveness, isp, retired, kaliscan, anisascans]
generated: { by: agent/mcp, at: "2026-11-19T14:05:20Z" }
status: stable
---

## Method

1. Call every live plugin through the daemon: `GET /plugin/<id>/search?q=one%20piece`
   (the `/search` page is a JS shell — curling it proves nothing).
2. For anything that failed, get global DNS truth from DoH
   (`https://dns.google/resolve?name=HOST&type=A`), then confirm by connecting
   straight to the DoH-resolved IP with SNI=host and a plain `GET /`.
3. A site is only "down" when a public resolver says NXDOMAIN/SERVFAIL.

## Verdict (2026-11-19)

- **Dead, removed from `data/plugins` (code kept in `plugins_dev/lua/`):**
  - `kaliscan.me` — NXDOMAIN on Google and Cloudflare DoH for A/NS/CNAME/MX.
  - `anisascans.in` — SERVFAIL, "No Reachable Authority at delegation" (EDE 22);
    the plugin was already disabled in the store.
- Also unlinked `enginetest` — a scratch probe plugin, never a real site.
- **Alive globally, failing only here:** `dynasty-scans.com`, `madaradex.org`,
  `mangadex.org` / `api.mangadex.org`, `fanfox.net` / `m.fanfox.net`,
  `ww3.mangafreak.me` — all answer 200/302 at the real IP; the local resolver
  returns a wildcard `*.ioh.co.id` cert, so libcurl fails certificate
  verification. `www.harimanga.cc` answers 403 (Cloudflare) — alive.
  Never retire a plugin on the strength of a local TLS error.
- **Alive but the plugin returns nothing** (separate defect, not a dead site):
  `comix`, `mangahub`, `valirscans` — 0 items for every query while the site
  serves a normal 200. `batcave`, `mangasushi`, `sanascans` return 0 for
  "one piece" too but work for other terms, so always retry with two queries
  before calling a plugin broken.

## Retirement rule

Removing the symlink in `data/plugins/` takes the plugin out of the runtime
(`/plugin/<id>/...` then answers `no plugin '<id>'`) while the code and its
history stay in `plugins_dev/` for reference. Check `saved_manga` first: no
library row may reference the retired id, or the grid keeps a card whose
detail page can no longer refresh. Purge the plugin's `api_cache` rows too.

# Related Concepts
- Related to [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](../decisions/deps-and-plugins.md)
