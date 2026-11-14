---
type: Fact
title: "xmake cxx_bin: def-time add_ldflags can drop flags; on_load t:add is the reliable route"
description: "In the shared cxx_bin helper, flags added via def-time add_ldflags(..., {force=true}) can silently miss the link command (-rdynamic was dropped while CURL_SO from the very same call landed); the proven route is t:add('ldflags', ..., {force=true}) inside the target on_load hook, the same path that injects -fuse-ld=mold."
tags: [xmake, build, ldflags, dynsym, cli]
generated: { by: agent/mcp, at: "2026-11-14T20:46:28Z" }
status: stable
---

## Symptom

`-rdynamic` placed in the cppIsekai cfg `ldflags` (helper line: `add_ldflags(table.unpack(cfg.ldflags), {force = true})`) never reached the link command: `xmake -v` showed `CURL_SO` and `-fuse-ld=mold` but no `-rdynamic`, and `nm -D` on the linked binary reported 0 defined `T` dynsym entries with `main` unexported. The same call delivered the `.so` path, so the drop is flag-specific, not call-wide.

## Fix

Inject via the target `on_load` hook - the route already used for the linker pick:

    on_load(function(t)
        import("lib.detect.find_program")
        if find_program("mold") then
            t:add("ldflags", "-fuse-ld=mold", {force = true})
        end
        t:add("ldflags", "-rdynamic", {force = true})
    end)

Verified: link cmd contains `-rdynamic`; `nm -D build/linux/x86_64/releasedbg/cppIsekai | grep -c ' T '` = 3550; backward crash-trace frames resolve by name.

## Audit recipe

    xmake -v <target> 2>&1 | grep -c -- '-rdynamic'   # flag reached the link cmd
    nm -D <binary> | grep -c ' T '                    # dynsym exports landed

## CLI verb forms (observed)

- `xmake build <tgt>` / `xmake -v <tgt>` - build (verbose) the target.
- `xmake <tgt>` - errors `invalid task: <tgt>`.
- `xmake -v build <tgt>` - errors `'build' is not a valid target name for this project`.

# Related Concepts
- [Crash traces via backward-cpp: fatal-signal handler in main, dynsym-only names, stderr capture](../decisions/crash-traces-backward-cpp.md): The on_load -rdynamic export documented here is what lets backward's dladdr resolver name crash-trace frames; without it the crash-trace decision degrades to bare addresses.
