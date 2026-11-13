---
type: Decision
title: "xrepo offline packages workflow: batch export + import -i packages (packages/ stays gitignored)"
description: "Dependency pins are saved in two layers (xmake.lua + committed xmake-requires.lock); offline dev installs use one-shot `xrepo export -o packages` / `xrepo import -i packages` instead of per-package exports, and packages/ stays gitignored (40 MB binaries shipped as tarball)."
tags: [xmake, xrepo, offline, dependencies, workflow]
generated: { by: agent/mcp, at: "2026-11-13T07:23:12Z" }
status: stable
---

# xrepo offline packages workflow (export/import)

Two-layer dependency pinning in this project:

1. **`xmake.lua` pins** — explicit versions in `add_requires(...)` (all 17 deps: glaze 7.9.0, date 3.0.4, hash-library 2021.09.29, tiny-aes-c 2021.12.22, re2 2025.11.05, ...).
2. **`xmake-requires.lock`** — xmake-generated lockfile (COMMITTED) that also pins every transitive dep (abseil, libzip, cmake/ninja, zlib) plus the exact xmake-repo commit hash, keyed per platform (`linux|x86_64`). Deleting it lets transitive deps drift.

## Fast batch export (not one-by-one)

```bash
# export ALL pins in ONE command (no per-package loop):
grep -oP '(?<=")[a-z0-9_-]+ [0-9][^"]*(?=\")' xmake.lua | xargs xrepo export -y -o packages
```

- `xrepo export -y -o packages "glaze 7.9.0" "date 3.0.4" ...` accepts many packages in one run.
- Without `--shallow` it also exports transitive deps. Measured 2026-11-13: 20 packages (17 pins + abseil/cmake/ninja/libzip) = **40 MB**.
- Layout: `packages/<letter>/<name>/<version>/<sha>/` containing `include/ lib/ manifest.txt share/` (installed-artifact store, NOT a recipe repo — no xmake.lua inside).

## Consuming on a fresh dev machine (offline)

```bash
xrepo import -i packages -y "glaze 7.9.0" "date 3.0.4" ...   # reads ./packages, pulls transitive deps too
xmake -y                                                       # then build normally
```

Verified round-trip 2026-11-13: `xrepo import -i packages` re-imported date + hash-library including their host deps (cmake, ninja) from the local folder.

## Decision: packages/ stays gitignored

`packages/` is deliberately in `.gitignore` (commit 245e3bd) — 40 MB of binaries in git history bloats every clone. Ship it as a release tarball asset when offline installs are needed; otherwise dev machines use the committed `xmake-requires.lock` via `xmake -y` (network required, exact versions guaranteed).

generated: { by: "orchestrator", at: "2026-11-13" }

# Related Concepts
- [xmake build system alongside cmake + prod-code gate tasks](xmake-build-system.md): Same build-system decision family: xmake primary, with this concept covering offline package pinning/export around it
- [Second xrepo round: pkgconfig deps + utf8proc NFKD tokenizer](xrepo-round-2-deps.md): Round-2 pins are the package set exported by this offline workflow
