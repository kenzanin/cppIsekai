---
type: Bug
title: Phantom test crashes were gdb-session artifacts; verify plain-run first
description: "test_plugin/test_htmltext SIGSEGVs seen only in gdb sessions (sol2 closure dtor, _Rb_tree::_M_erase, garbage this) never reproduced on plain runs; suite 18/18 green twice - dig backtraces only on a plain-run repro."
tags: [testing, gdb, releasedbg, doctest, verification, crash]
generated: { by: agent/cli, at: "2026-11-16T06:02:19Z" }
status: stable
---

2026-11-16: test_plugin and test_htmltext SIGSEGVed deterministically across two gdb batch sessions and two code states (with/without date dep, pre/post lua-capture-fix): sol2 user_alloc_destroy functor dtor, _Rb_tree::_M_erase on a static map, this=0x0 / 0x0f-fill pointers. Never reproduced on plain runs: after a fresh rebuild both binaries and the full 18-target doctest suite exit 0, twice, with the lua capture fix restored (A/B via git stash + rebuild + plain run). Verdict: backtrace-only crash = gdb-session/stale-binary artifact; root cause never pinned, state green and reproducible. Standing rule: re-run the plain binary once on a fresh build before any gdb session; only dig backtraces on a plain-run repro.
