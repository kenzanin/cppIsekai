---
type: Fact
title: "Delegating to a free model: native subagent skill beats spawn_agents (no model field)"
description: "Free-model work can be delegated two verified ways (native Reasonix skill with runAs:subagent + model 9router/free, or opencode run -m 9router/free), while the spawn_agents tool has no model field at all; configured model names live in ~/.config/opencode/opencode.json."
tags: [tooling, subagent, delegation, free-model, opencode, reasonix]
generated: { by: agent/mcp, at: "2026-11-14T10:19:17Z" }
status: stable
---

Verified 2026-11-14 by probing each path.

## Two working paths to a free model

| path | how | model selection |
| --- | --- | --- |
| native Reasonix subagent | `install_skill(runAs:"subagent", model:"9router/free")` then call the skill | explicit, pinned per skill |
| opencode CLI | `opencode run -m 9router/free --auto --archived "<task>"` | explicit, per invocation |

Both were probed with `Reply with exactly: OK` and both returned `OK`. The native path is preferred (no external process, no session bookkeeping).

## `spawn_agents` cannot pick a model

`tool:spawn_agents` (alias `task:subagent`) accepts only `{title, task, tools?}` — there is NO model field, so those agents run on the parent session's model. To use a specific model, go through a skill with `runAs: subagent` + `model`, or through the opencode CLI.

## Configured models

Provider/model names live in `~/.config/opencode/opencode.json`. Enumerate them with a JSON walk over `provider.*.models`. At probe time: `9router/{designer,explorer,fixer,librarian,local,oracle,orchestrator,vision,free}` and `glm/GLM-5.3-Flash-EXL3`. `9router/free` is the free one.

Note the harness's own provider set is different (an error during this session mentioned `zai/glm-5.3-flash-max`), so an opencode model name is not automatically a harness model name — but the native skill accepted `9router/free` anyway.

## Skills installed

- `~/.reasonix/skills/free-worker/SKILL.md` — `runAs: inline`, drives the opencode CLI.
- `~/.reasonix/skills/free-worker-native/SKILL.md` — `runAs: subagent`, `model: 9router/free`, `effort: high`, tools `read_file`/`write_file`/`bash`.

## Practical cautions

- The subagent never sees the parent conversation: give absolute paths, the goal, hard constraints and the acceptance checks.
- `--auto` on the opencode path approves everything not explicitly denied; keep destructive steps spelled out in the task text.
- Builds and test suites outlive a foreground call: run them in the background and poll.
- A subagent's report is self-reported. Check `git status` / `git diff` and re-run the decisive test before believing it.
- A long opencode run can be interrupted mid-flight and leave a clean working tree plus only a created branch; verify actual file changes before assuming work happened.

# Related Concepts
- [status-flags apply: backfill must precede DROP COLUMN has_update (18/31 rows at risk)](status-flags-apply-migration-order.md): The status-flags apply was delegated to the free worker; this finding records the delegation routing used for it.
