---
type: decision
title: "Tray icon: SNI IconName must be an absolute path for xfce4-panel's status tray"
description: "The daemon's StatusNotifierItem exported IconName=\"assets/tray.png\" (relative) with no IconPixmap, so xfce4-panel's systray host looked it up as a theme name and rendered nothing on X11; resolving the path to absolute before app_indicator_new makes the item render (XFCE loads absolute IconNames straight from file). Wayland hosts accepted the relative form, which is why the same build showed a tray there and none on XFCE/X11."
tags: [tray, appindicator, statusnotifier, xfce, x11, icon]
generated: { by: agent/mcp, at: "2026-11-19T06:58:36Z" }
status: stable
---

## Symptom

XFCE 4.20 / X11: the tray item registered on the session bus
(`org/ayatana/NotificationItem/cppIsekai` appeared in the
`org.kde.StatusNotifierWatcher` RegisteredStatusNotifierItems list) but no icon
ever appeared in the panel, for any of the icon variants. The same binary under
a Wayland session showed the icon normally.

## Evidence

Panel host: xfce4-panel 4.20.8, plugin `libsystray.so` ("Status Tray Plugin —
provides status notifier items (application indicators) and legacy systray
items") running as `wrapper-2.0`, and it owns `org.kde.StatusNotifierWatcher`.
So a proper SNI host existed; registration was never the problem.

Item properties before the fix:

    IconName      = "assets/tray.png"        <- relative, no IconPixmap exported
    IconThemePath = ""

A reference item on the same bus (blueman) exported a theme name
(`IconName = "blueman-tray"`). The host resolves IconName through the icon
theme unless it is an absolute path, so `assets/tray.png` resolved to nothing.

## Decision

`src/tray.hpp` resolves the icon to an absolute path
(`std::filesystem::absolute`, falling back to the relative string when the file
is missing) before constructing the traypp Tray. Absolute IconNames are loaded
directly from file by xfce4-panel's sn-item, which is the path its own
appindicator clients rely on.

## Verification

- Bus: `IconName = "/home/.../assets/tray.png"` after restart.
- Pixels: panel window (window id at 0,831 1536x33) captured before and after;
  the amber book icon appears in the systray between the network indicator and
  thunderbird after the fix, absent before.
- `xmake test` 19/19.

## Rejected alternatives

- Theme-name icon (`IconName = "cppisekai"` + install into
  `~/.local/share/icons/hicolor`): correct for a packaged install, but the
  daemon runs from a source tree with no install step, and writing into the
  user's icon theme at runtime is intrusive.
- `IconThemePath`: KDE honours it, xfce4-panel's host ignores it, so it would
  not fix this case.
- GtkStatusIcon/XEmbed fallback: unnecessary once the absolute IconName resolves;
  XEmbed is also a dead end on Wayland.
- Shipping IconPixmap explicitly: traypp/libappindicator only fill it from a
  GtkImage, and it is not needed for either host.
