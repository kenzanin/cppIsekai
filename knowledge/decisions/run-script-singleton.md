---
type: decision
title: "run-cppIsekai.sh: flags --close/--restart only, bare run is singleton-guarded"
description: "Single canonical CLI: no arg = guarded foreground run (refuses + hints when a daemon lives, killing the double-bind EADDRINUSE class), --close = the old stop path (TERM->KILL->chrome/xvfb sweep->socket wait), --restart = close then exec. No positional aliases, no duplicate stop verbs."
tags: [ops, daemon, script, singleton, lifecycle]
generated: { by: agent/mcp, at: "2026-11-27T15:27:40Z" }
status: stable
---

# Decision

One verb per action, flag-style only:

- bare / run: if pgrep -x cppIsekai hits, print pid + two hint lines, exit 0; else exec the daemon.
- --close: TERM, wait, KILL fallback, orphaned chrome + xvfb sweep, wait :8090 out, kill stale fd holders.
- --restart: --close then exec.

Rationale: the 2026-11-27 wedged-daemon session came from ad-hoc nohup+pkill
races around the script; a guarded single entry point makes double instances
impossible by construction. The old positional stop|restart are gone -- the
guard message teaches the flags, nothing else needs to remember them.

# Note

The xvfb sidecar can hold the listen fd after daemon death; the socket-wait +
fd-holder kill in --close stays mandatory before any re-bind.

# Related Concepts
- [Plugin fleet scan 2026-11-27: 3 real breaks, 1 upstream outage, plain-TLS landmine](../findings/render-scan-2026-11-27.md): wedge incident that motivated the guard
