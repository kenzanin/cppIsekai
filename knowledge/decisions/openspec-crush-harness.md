---
type: Decision
title: "OpenSpec harness wiring: Crush gets .crush/ (local), universal copy stays in .agents/"
description: "openspec init --tools crush writes 6 skills to .crush/skills and 6 slash commands to .crush/commands/opsx; both dirs are gitignored by Crush's own .crush/.gitignore, so the tracked universal copy in .agents/skills is kept and refreshed with openspec update --force. The only difference between the two copies is the closing prompt (/opsx:apply vs /openspec-apply-change)."
tags: [openspec, crush, harness, tooling, skills]
generated: { by: agent/crush, at: "2026-11-16T00:47:55Z" }
status: stable
---

Setup: openspec 1.16.1, `openspec init --tools crush --no-animation`.

What the crush target writes:
- .crush/skills/openspec-{apply-change,archive-change,explore,propose,sync-specs,update-change}/SKILL.md -- loaded by Crush (its skill loader scans .crush/skills, .agents/skills, .claude/skills, .cursor/skills).
- .crush/commands/opsx/{apply,archive,explore,propose,sync,update}.md -- Crush custom commands, so the entry point is /opsx:propose.
- Nothing in AGENTS.md; openspec/config.yaml untouched.

Why .crush/ is not committed: .crush/.gitignore contains `*` (Crush's data dir also holds crush.db, logs). Do not fight it; the harness files are regenerable with `openspec init --tools crush`.

Duplication is deliberate and small: .agents/skills/openspec-* is the tracked universal copy (`other / universal (shared .agents skills)` target, also what OpenCode reads). After this setup the two copies differ on exactly one line per skill -- the closing prompt names /opsx:apply on the Crush side and /openspec-apply-change on the universal side. Keep both; do not hand-merge.

Refresh procedure: `openspec update --dry-run --json` to see what changes, then `openspec update --force`. `openspec doctor` reports the root as ok.

Verified after setup: openspec validate --specs green, openspec doctor root ok, openspec list --json returns the two in-flight changes (plugin-port-multi-runtime, refactor-hygiene).

# Related Concepts
- [Test and lint tooling (doctest, clang-tidy, clang-format)](../references/tooling.md): Tooling reference now also covers the OpenSpec harness dirs
