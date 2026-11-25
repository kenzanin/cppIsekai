---
type: Decision
title: "plugin show/hide ditunda: toggle enabled cukup"
description: "Hide-from-UI flag untuk plugin test ditunda; toggle enabled cukup, upgrade path 3 titik sudah dipetakan."
tags: [plugins, ui, deferred, testing]
generated: { by: agent/mcp, at: "2026-11-25T17:51:32Z" }
status: stable
---

## Context
Daemon dipakai juga buat testing plugin buatan LLM; eksperimen setengah jadi menumpuk di dropdown search. Usulan: flag sembunyi-dari-UI per plugin (`show=off`).

## Keputusan
**Ditunda; toggle Enable/Disable yang ada sudah cukup** (`POST /plugin/<int>/enabled`; dropdown search sudah filter `p.enabled` — `templates/search_script.mustache:12`). Tradeoff diterima pemilik: plugin disabled **tidak bisa dipanggil** (`/plugin/<id>/<action>` → 404 "plugin disabled"), jadi siklus test = enable → test → disable.

## Pemicu untuk membuka lagi
Dropdown penuh eksperimen LLM sampai mengganggu. Upgrade path sudah dipetakan, 3 titik:
1. `std::optional<bool> show` di `struct Manifest` (`src/plugin/plugin_internal.hpp:61`, default true)
2. expose `hidden` di row `/api/plugins` (`src/http/http_api_admin_plugins.cpp` ~:95)
3. filter `&& !p.hidden` di `templates/search_script.mustache:12` (page Plugins tetap menampilkan semua)

Biaya: sentuh C++ → `xmake -y` + restart daemon. Jangan tambah sebelum pemicu kena.

generated: { by: "kit-agent", at: "2026-11-26" }

# Related Concepts
- [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md): visibility flag would extend the plugin manifest/host surface; deferred by this decision
