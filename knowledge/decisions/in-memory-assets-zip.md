---
type: Decision
title: "Assets: optional in-memory zip/folder root with read-only ini knob"
description: "[assets] path loads templates/static/icon from a zip or folder into RAM at boot; empty path keeps disk layout; knob is read-only in UI and HTTP."
tags: [http, static-assets, config, architecture, offline-first]
generated: { by: agent/mcp, at: "2026-11-24T05:35:34Z" }
status: stable
---

## Contract

- `[assets] path` (registry idx 30, env CPPISEKAI_ASSETS_PATH, no CLI flag, `readonly=true`): a .zip file or a folder containing `templates/`, `static/`, `icon/`. Empty = current repo layout on disk.
- `src/assets/assets.cpp` loads wanted entries once at boot into an in-memory map and installs `crow::mustache::detail::get_loader_ref()` so partials (`{{>top.mustache}}`) resolve from memory too. `br_asset` prefers `<key>.br` from memory; `assets::icon_path` writes the in-memory icon to one temp file because GTK needs a real path; `assets::shutdown` restores the loader/removes the temp file.
- Read-only enforced twice: `readOnly(k)` in settings_script + `k.readonly` from GET /config, and POST /config erases registry-declared read-only keys before save.
- `xmake pack-assets` builds `assets.zip` (gitignored) from templates/ + static/ + icon/tray.png.
- libzippp already a dependency; no new package. 32 MB root / 8 MB file ceilings; path traversal avoided by key-based lookup (no extraction to disk except the single icon).

## Why

Single archive for offline distribution, no CDN/runtime; in-memory reads avoid per-request disk I/O. Chosen over tar.zst (no random access + new dep) and over temp-dir extraction (extra disk state). Rejected changing the knob live: asset set is fixed at boot, so UI shows read-only + restart note.

## Verification (2026-11-23)

21/21 tests including new `test_assets` (zip + folder mode, mustache partial from memory, icon temp cleanup). Live: loaded 36 entries from assets.zip, detail page rendered 200 with BOTH `templates/detail.mustache` and `static/app.css.br` physically moved away; css 200 text/css 6309 B brotli. GET /config exposes readonly=true; POST /config {assets.path:/evil} wrote 0 keys; browser settings input disabled with 'read-only — edit the ini and restart'. Restored disk mode (path empty) and daemon healthy.

# Related Concepts
- [Static assets: vendored, precompressed brotli-only via explicit routes](static-assets-br.md): supersedes disk-only brotli asset serving with an optional in-memory zip/folder root; .br bodies are unchanged
- [cli-config: registry-backed config surface (CLI/env/ini/dump/save)](../architecture/cli-config.md): adds one registry knob with a new readonly flag surfaced through describe() and GET /config


## Curation update (2026-11-24)

`xmake pack-assets` now packs all templates + **every brotli body the app serves**, including `marked.min.js.br` and `mermaid.min.js.br` for /about. It drops plain `.js`/`.css` build sources and **never packs the tray/favicon PNG** (1.3 MB curated archive vs 90 KB earlier icon-less subset). The icon is always resolved from `[tray] icon` on disk — `assets::icon_path` no longer writes a temp copy, and `favicon_response` reads `assets/tray.png` from disk. Verified: zip has 0 icon entries, `/favicon.ico` still 200 image/png 5790 B byte-identical to `assets/tray.png`, `/static/mermaid.min.js` 200 with 1,187,603 B brotli from memory, `/about` 200. 31 entries loaded.

## Favicon (2026-11-24)

`/favicon.ico` and `/favicon.png` both serve the **same PNG as the systray** (`assets/tray.png`), looked up from the memory map first (`icon/tray.png` or `assets/tray.png`) then disk fallback. All three head templates emit `<link rel="icon" type="image/png" href="/favicon.png">`. Verified byte-identical MD5 (5790 B) in both zip and disk mode.
