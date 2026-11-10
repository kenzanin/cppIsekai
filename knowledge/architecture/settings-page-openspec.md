---
type: Decision
title: "settings-page: openspec change drafted (GET /config projection + /settings UI)"
description: "Settings page shipped and archived 2026-11-10 (applied + committed b884ff7, templates_dir knob added aa1e213, daemon on :8090 restarted with new build). Archived via openspec archive settings-page (-y): delta merged into openspec/specs/settings-page/spec.md, Purpose filled in, --specs validate clean (17/17). GET /config projects the registry with layer provenance; /settings UI; edits via existing POST /config merge-save."
tags: [config, ui, openspec, settings, proposal]
generated: { by: agent/mcp, at: "2026-11-10T11:09:26Z" }
status: stable
---

Shipped 2026-11-10 and archived same day. Proposal/design/specs/tasks applied; 9/9 tasks complete; openspec validate clean; ctest 14/14; live smoke on :8124 and :8090. Commits b884ff7 (apply), aa1e213 (paths.templates_dir knob + settings restart badge). Main spec: openspec/specs/settings-page/spec.md. Archive dir: openspec/changes/archive/2026-11-10-settings-page/.
