---
type: Decision
title: xmake build system alongside cmake + prod-code gate tasks
description: "xmake is the primary build path (cmake kept, ./build rebuilt by xmake). New flow: xmake build → xmake test → xmake gate (format/fix/build/test, prod src only). Includes daily commands + 8 gotchas. Committed d019d91/3a8a466/12b7202."
tags: [build, xmake, cmake, clang-tidy, clang-format, gate, workflow]
generated: { by: agent/mcp, at: "2026-11-12T04:15:47Z" }
status: stable
---

Decision (2026-11-12, committed d019d91/3a8a466/12b7202): xmake is the primary build path; CMakeLists.txt stays for reference but the cmake ./build dir was deleted and rebuilt by xmake (build/ now holds only the xmake layout: build/linux/x86_64/release/<target>).

Files: xmake.lua (new), .gitignore (+.xmake/ +compile_commands.json), run-cppIsekai.sh (points at build/linux/x86_64/release/cppIsekai).

## New daily flow
- Build all (daemon + 16 tests): `xmake -j` (ccache automatic, mold detected in on_load).
- Test: `xmake test` (16/16, parity with ctest).
- Done gate before declaring work finished: `xmake gate` = format (clang-format -i) → fix (clang-tidy -fix -fix-errors) → build → test. Individual: `xmake format` / `xmake tidy` (report only) / `xmake fix`.
- Gate scope: prod code only (src/**); tests/ and third_party/ are never auto-edited.
- Run daemon: `./run-cppIsekai.sh` or nohup the binary with `--config data/cppisekai.ini`; restart after rebuilding before judging template bugs (templates render from disk, endpoints come from the binary).
- compile_commands.json at project root for clangd/clang-tidy: `xmake project -k compile_commands` (also run by tidy/fix tasks).
- clang-tidy volume is huge on heavy template TUs (~25k generated/file, suppressed by HeaderFilterRegex to src/*.hpp); WarningsAsErrors stays empty so tidy reports but never blocks. Review `xmake fix` diffs deliberately — modernize-* rewrites en masse.

Target parity: vendored static C libs (wasm3, quickjs, lexbor), daemon cppIsekai (src/**.cpp + libzippp), 16 doctest binaries via add_tests. First `xmake format` run rewrote 55 prod files (+1204/−893) — codebase had drifted from .clang-format (120 cols); rebuild + tests stayed green.

## Gotchas hit
1. import() is script-domain only (on_load/on_run) — not available at xmake.lua root scope. mold detection lives in on_load.
2. pkg-config deps go through add_requires("pkgconfig::X", {system=true}) + add_packages — no add_pkgconfig in xmake 3.1.2.
3. add_files needs flat string lists (no nested tables).
4. set_languages("c23") is strict ISO: hides POSIX symbols (strnlen in wasm3) that cmake's default gnu23 exposed — vendor C libs need _GNU_SOURCE.
5. Task on_run executes in a fresh Lua sandbox: globals defined in xmake.lua are NOT visible; os.exec does NOT emulate shell pipes (passes | as argv). Use os.files + os.execv inline in on_run.
6. Tasks need set_menu to be invocable as `xmake <name>` (see xmake-custom-plugins skill).
7. Appending to .gitignore without a trailing newline glues entries (".xmake/compile_commands.json") — check-ignore with `git check-ignore -v` after editing.
8. xmake-skills plugin package installed globally from github.com/xmake-io/xmake-skills (58 skills, commit ef67caa) — reference for xmake idioms.
