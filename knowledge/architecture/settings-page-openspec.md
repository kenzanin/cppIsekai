---
type: Decision
title: "settings-page: openspec change drafted (GET /config projection + /settings UI)"
description: "OpenSpec change 'settings-page' drafted + validated 2026-11-10 (proposal/design D1-D7/spec delta/tasks). Web UI Settings page: GET /config projects the registry with layer provenance (default|ini|env|flag, read-only when env/flag wins), edits go through existing POST /config merge-save (comments preserved, plugin.fetch_profile stays live), restart badges for non-live knobs, nav entry after Plugins."
tags: [config, ui, openspec, settings, proposal]
generated: { by: agent/mcp, at: "2026-11-10T10:13:35Z" }
status: stable
---

Not implemented yet. Key rulings: D1 GET /config projects the registry (helper in src/config/, re-resolved per request), D2 edit path is existing POST /config unchanged, D3 layer provenance surfaced as read-only rows, D4 restart hints not auto-restart, D5 enum selects for image_format/log.level, D6 proxy treated as password field, D7 nav after Plugins. Validation: `openspec change validate settings-page` clean. Tasks: 1.1-1.3 read projection + config::describe helper + unit test, 2.1-2.3 settings.mustache + /settings route + nav, 3.1-3.3 verify.
