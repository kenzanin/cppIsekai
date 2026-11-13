---
type: constraint
title: xmake pkg-install interrupted leaves empty hash-dirs
description: "Killing xmake during package install (timeout or Ctrl-C) leaves EMPTY hash-dirs under ~/.xmake/packages/<pkg>/<version>/<hash>/. Compile then runs with missing -I and fails on random headers; project config cache (.xmine) keeps stale pkg paths. Recovery: rm -rf ~/.xmake/packages ~/.xmake/cache/packages, then xmake f -c -y (clean reconfig re-resolves+reinstalls). Never kill xmake during its install phase."
tags: [xmake, constraint, recovery, packages]
generated: { by: agent/mcp, at: "2026-11-13T00:44:17Z" }
status: stable
---


