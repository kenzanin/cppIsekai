---
type: reference
title: "Daemon run & verify (run-cppIsekai.sh)"
description: "How to (re)start the cppIsekai daemon and verify library state live — correct binary path, stale-CMake-binary trap, and two grep false-alarm patterns."
tags: [daemon, run, verify, grid]
generated: { by: agent/mcp, at: "2026-11-13T11:22:59Z" }
status: stable
---

# Daemon run & verify

## Start / restart
```bash
cd <repo> && ./run-cppIsekai.sh        # = ./build/linux/x86_64/release/cppIsekai --config ./data/cppisekai.ini
# background: nohup ./run-cppIsekai.sh > /tmp/opencode/cppIsekai.log 2>&1 &
pgrep -af 'build/.*/cppIsekai'         # confirm the xmake binary path is running
```

**Stale-binary trap:** a leftover `./build/cppIsekai` (old CMake artifact, path #5310) may still be the running process — it can predate recent features. Always check `pgrep -af` output matches `build/linux/x86_64/release/`, kill the old PID first, then start via the script.

## Verify library is alive (without a browser)
- `curl -s http://localhost:8090/ | grep -o '<li' | wc -l` → saved-card count (24 in the 2026-11-13 state)
- `curl -s 'http://localhost:8090/api/library?page=1'` → JSON rows (store-level truth)
- HTTP codes: `/` and `/library/grid` must be 200; log free of error/fail lines.

## Two grep false alarms (do not chase these)
1. Log line `library plugin='' q='' page 1 -> 0 titles` is EXPECTED: `titles` is the legacy search-cache list; the grid renders from `saved_list` (`saved_manga`, 24 rows) — see `http_pages.cpp` comment "saved entries are the Library".
2. Card links render as `href="&#x2F;view&#x2F;manga&#x2F;..."` (mustache escapes `/` as `&#x2F;`), and cards carry `data-plugin`, NOT `data-grid-plugin` (that attr is sidebar-only). Grepping for `view/manga` or counting `data-grid-plugin` on cards therefore returns 0/1 even when the library is fully rendered.

# Related Concepts
- [host-tool gap analysis](host-tool-gap-analysis.md): daemon restart on 2026-11-13 verified the P1–P9 host tools live.
