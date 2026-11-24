---
type: Decision
title: ".desktop launcher: templated Exec/Path/Icon + install script"
description: "Committed packaging/cppIsekai.desktop uses @APP_DIR@ placeholders; scripts/install-desktop.sh substitutes the checkout path into ~/.local/share/applications/cppIsekai.desktop."
tags: [packaging, desktop, linux, launcher]
generated: { by: agent/mcp, at: "2026-11-24T02:04:23Z" }
status: stable
---

## Contract

- `packaging/cppIsekai.desktop` is path-independent: `@APP_DIR@` in `Exec`, `Path`, `Icon` and the three desktop actions (OpenReader / Restart / Stop).
- `scripts/install-desktop.sh` resolves the checkout root, substitutes `@APP_DIR@`, writes `~/.local/share/applications/cppIsekai.desktop`, chmod 644 and refreshes the desktop database when available.
- Icon is the same `assets/tray.png` as the systray and the HTTP favicon; Terminal=true because the default action runs the daemon in the foreground; Categories=Network only (validator hint on multiple main categories).

## Why

Desktop Entry `Exec` needs an absolute program path, but the repo can move; a placeholder + installer keeps the committed artifact portable and the installed one working. Actions mirror the tray menu so both entry points expose the same operations.

## Verification (2026-11-24)

`desktop-file-validate` clean, `gio info` reads the installed file, Exec/Path/Icon all resolve to existing files.

# Related Concepts
- [cli-config: registry-backed config surface (CLI/env/ini/dump/save)](../architecture/cli-config.md): desktop Exec reuses the canonical --config launcher instead of duplicating boot flags
