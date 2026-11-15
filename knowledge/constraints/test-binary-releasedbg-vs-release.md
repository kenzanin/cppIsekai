---
type: Fact
title: xmake test runs releasedbg; hand-run release test binaries go stale
description: "xmake test always executes build/linux/x86_64/releasedbg/test_*; the release/ tree is only rebuilt by an explicit release-mode build, so hand-running release test binaries validates stale code and produces phantom passes/failures."
tags: [xmake, build, testing, gotcha]
generated: { by: agent/mcp, at: "2026-11-15T05:42:31Z" }
status: stable
---

## Fact

`xmake test` executes `build/linux/x86_64/releasedbg/<target>` (strace-verified exec path). The default `xmake -y` after a mode switch rebuilds only the configured mode's tree; `build/linux/x86_64/release/test_*` then holds an older binary.

## Consequence

Hand-running a `release/` test binary "passes" or "fails" against stale code. During manga-detail-table this produced a phantom runner-vs-standalone discrepancy: the runner (releasedbg, current) failed on a real bug while the hand-run (release, stale) passed.

## Rule of thumb

Trust only `xmake test` verdicts, or rebuild the exact tree first (`xmake f -m release && xmake` for the daemon, which also refreshes `release/test_*`). The daemon at `build/linux/x86_64/release/cppIsekai` is a separate artifact: after test-only iterations it can still be stale - check mtime before restart.

Related: constraints/xmake-install-interrupt, decisions/xmake-build-system.
