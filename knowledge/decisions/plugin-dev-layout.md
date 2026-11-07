---
type: decision
title: "Plugin dev layout: plugins_dev/<lang>/<site> + symlink deploy"
description: "Plugin development happens in plugins_dev/<lang>/<site>/; testing symlinks/copies the plugin into data/plugins/; host ini carries no plugin sections."
tags: [plugin-system, workflow, config]
generated: { by: agent/mcp, at: "2026-11-07T03:04:31Z" }
status: stable
---

## Rule

Plugin sources live in `plugins_dev/<lang>/<site>/` (e.g. `plugins_dev/lua/mangadex/`, `js/` and `wasm/` reserved for future runtimes). For testing, a plugin is exposed to the host by symlink or copy into `data/plugins/<id>/` — the host always loads from `data/plugins` (its default), no ini override. Per-plugin secrets live in `plugins_dev/**/config.json` (gitignored); `data/cppisekai.ini` stays host-only and carries no per-plugin sections.

## Why
User decision 2026-11-07: separates plugin development from the deployed runtime dir, keeps one load path (`data/plugins`), and keeps the ini free of plugin data (see `decisions/plugin-data-segregation`).

# Related Concepts
- [Plugin data segregation: host .ini is host-only](plugin-data-segregation.md): Dev layout realizes the data-segregation rule at the workflow level.
