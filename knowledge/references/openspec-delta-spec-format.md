---
type: reference
title: openspec delta spec format (archive gate)
description: "openspec archive/validate only parses delta specs written as '### Requirement:' + SHALL prose + '#### Scenario:' WHEN/THEN blocks; other shapes are rejected."
tags: [openspec, spec, tooling]
generated: { by: agent/mcp, at: "2026-11-13T11:22:59Z" }
status: stable
---

# openspec delta spec format (archive gate)

`openspec archive <change>` (and delta validation) rejects a `specs/<capability>/spec.md` that does not use this exact shape:

```markdown
## ADDED Requirements

### Requirement: <short name>
<prose containing SHALL/MAY...>

#### Scenario: <name>
- **WHEN** <condition>
- **THEN** <expected>
```

Rules learned the hard way (host-tool-gap archive, 2026-11-13):
- Custom headings like `### REQ-HOST-TEXT: ...` are NOT parsed → "no requirement entries parsed" → archive aborts.
- No prose directly under `## ADDED Requirements` (it is dropped with a warning).
- `## REMOVED Requirements` accepts bullet-list syntax, but only for requirements that already exist in the main spec — do NOT put never-added or implemented items there (P8 skip lives in proposal/design/tasks instead).
- Each requirement needs ≥1 `#### Scenario:` block or validation fails with "no deltas found".
- Working reference: compare against any file under `openspec/changes/archive/*/specs/*/spec.md`.
- Gate order before archive: all tasks checked → `openspec validate --specs` → `openspec archive <name> -y` (creates/updates `openspec/specs/<capability>/spec.md`).

# Related Concepts
- [host-tool gap analysis](host-tool-gap-analysis.md): the change whose first archive attempt hit this format gate.
